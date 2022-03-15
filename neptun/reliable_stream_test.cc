//
// Created by freezing on 14/03/2022.
//

#include <gtest/gtest.h>
#include <vector>
#include <string>

#include "common/types.h"
#include "network/io_buffer.h"
#include "neptun/reliable_stream.h"

using namespace freezing;
using namespace freezing::network;

namespace {

static constexpr u16 kPacketId = 13;

byte_span span_of_string(const std::string &s) {
  return byte_span((u8 *) s.data(), s.size());
}

std::string string_of_span(byte_span span) {
  std::string s;
  for (u8 byte : span) {
    s += (char) byte;
  }
  return s;
}

}

std::vector<u8> make_buffer(usize size = 1600) {
  return std::vector<u8>(size);
}

TEST(ReliableStreamTest, BufferIsEmptyAfterWriteIfNoMessages) {
  auto buffer = make_buffer();
  ReliableStream stream{};
  auto count = stream.write(kPacketId, buffer);
  ASSERT_EQ(count, 0);
}

TEST(ReliableStreamTest, BufferIsEmptyAfterSendNoMessage) {
  ReliableStream stream{};
  stream.send([](byte_span buffer) {
    // We pretend that our message is too big and can't fit in the buffer,
    // so we don't write anything and return an empty span.
    return byte_span{};
  });
  auto buffer = make_buffer();
  auto count = stream.write(kPacketId, buffer);
  ASSERT_EQ(count, 0);
}

TEST(ReliableStreamTest, NothingToReadFromBufferWithoutReliableSegment) {
  auto buffer = make_buffer();
  ReliableStream stream{};
  usize msg_count = 0;
  auto byte_count = stream.read(kPacketId, buffer, [&msg_count](byte_span data) {
    msg_count++;
  });
  ASSERT_EQ(byte_count, 0);
  ASSERT_EQ(msg_count, 0);
}

TEST(ReliableStreamTest, SentMessageIsWrittenToPacket) {
  ReliableStream stream{};

  stream.send([](byte_span buffer) {
    auto io = IoBuffer{buffer};
    std::string msg = "foo is test for bar";
    usize count = io.write_byte_array(span_of_string(msg), 0);
    return buffer.first(count);
  });

  auto underlying_buffer = make_buffer();
  byte_span buffer{underlying_buffer};

  auto byte_count = stream.write(kPacketId, buffer);
  ASSERT_GT(byte_count, 0);
  auto segment = Segment(buffer);
  ASSERT_EQ(segment.manager_type(), ManagerType::RELIABLE_STREAM);
  ASSERT_EQ(segment.message_count(), 1);
  auto io = IoBuffer{buffer};
  ASSERT_EQ(io.read_u16(Segment::kSerializedSize), strlen("foo is test for bar"));
  ASSERT_EQ(string_of_span(io.read_byte_array(Segment::kSerializedSize + sizeof(u16),
                                              strlen("foo is test for bar"))),
            "foo is test for bar");
}

TEST(ReliableStreamTest, WriteThenReadMessage) {
  ReliableStream server_stream{};
  ReliableStream client_stream{};

  client_stream.send([](byte_span buffer) {
    auto io = IoBuffer{buffer};
    std::string msg = "foo is test for bar";
    usize count = io.write_byte_array(span_of_string(msg), 0);
    return buffer.first(count);
  });

  auto buffer = make_buffer();

  auto write_count = client_stream.write(kPacketId, buffer);
  ASSERT_GT(write_count, 0);

  usize msg_count = 0;
  usize read_count = server_stream.read(kPacketId, buffer, [&msg_count](byte_span payload) {
    msg_count++;
    ASSERT_EQ(string_of_span(payload), "foo is test for bar");
  });
  ASSERT_EQ(read_count, write_count);
  ASSERT_EQ(msg_count, 1);
}

TEST(ReliableStreamTest, PacketDeliveryStatus) {
  constexpr auto kDroppedPacketId = kPacketId;
  constexpr auto kUnresolvedPacketId = kPacketId + 1;
  constexpr auto kSuccessfulPacketId = kPacketId + 2;
  constexpr auto kAfterSuccessPacketId = kPacketId + 2;

  ReliableStream server_stream{};
  ReliableStream client_stream{};

  client_stream.send([](byte_span buffer) {
    auto io = IoBuffer{buffer};
    std::string msg = "foo is test for bar";
    usize count = io.write_byte_array(span_of_string(msg), 0);
    return buffer.first(count);
  });

  // ClientStream successfully writes to the buffer.
  auto dropped_packet_buffer = make_buffer();
  auto dropped_write_count = client_stream.write(kDroppedPacketId, dropped_packet_buffer);
  ASSERT_GT(dropped_write_count, 0);

  // Since the message is still in-flight, the client won't write the same message again.
  auto unresolved_packet_buffer = make_buffer();
  ASSERT_EQ(client_stream.write(kUnresolvedPacketId, unresolved_packet_buffer), 0);

  // Let's pretend that the corresponding packet was not delivered.
  client_stream.on_packet_delivery_status(kDroppedPacketId, PacketDeliveryStatus::DROP);

  // Finally, write to the successful packet.
  auto successful_packet_buffer = make_buffer();
  auto successful_write_count = client_stream.write(kSuccessfulPacketId, successful_packet_buffer);
  ASSERT_GT(successful_write_count, 0);

  usize msg_count = 0;
  usize read_count =
      server_stream.read(kPacketId, successful_packet_buffer, [&msg_count](byte_span payload) {
        msg_count++;
        ASSERT_EQ(string_of_span(payload), "foo is test for bar");
      });
  ASSERT_EQ(read_count, dropped_write_count);
  ASSERT_EQ(msg_count, 1);

  // Notify the stream that the packet is successful, which ensures that the message that has
  // been delivered successfully won't be sent again.
  client_stream.on_packet_delivery_status(kSuccessfulPacketId, PacketDeliveryStatus::ACK);
  auto after_success_buffer = make_buffer();
  ASSERT_EQ(client_stream.write(kAfterSuccessPacketId, after_success_buffer), 0);
}

TEST(ReliableStreamTest, WritesMultipleMessagesPerPacketEachTime) {
  ReliableStream server_stream{};
  ReliableStream client_stream{};

  constexpr usize kTotalMessages = 10;

  for (int i = 0; i < kTotalMessages; i++) {
    client_stream.send([i](byte_span buffer) {
      auto io = IoBuffer{buffer};
      std::string msg = "foo is test for bar " + std::to_string(i);
      usize count = io.write_byte_array(span_of_string(msg), 0);
      return buffer.first(count);
    });
  }

  // Create buffer such that at most 6 messages can fit.
  // Each message is [strlen("foo is test for bar X")] long, with a [sizeof(u16)] overhead that
  // ReliableStream adds.
  constexpr usize kMaxNumMessagesPerPacket = 6;
  const usize kMessageSize = (sizeof(u16) + strlen("foo is test for bar X"));
  auto buffer =
      make_buffer(Segment::kSerializedSize
                      + kMaxNumMessagesPerPacket * kMessageSize);

  auto write_count = client_stream.write(kPacketId, buffer);
  ASSERT_GT(write_count, 0);

  usize msg_count = 0;
  usize read_count = server_stream.read(kPacketId, buffer, [&msg_count](byte_span payload) {
    ASSERT_EQ(string_of_span(payload), "foo is test for bar " + std::to_string(msg_count));
    msg_count++;
  });
  ASSERT_EQ(read_count, write_count);
  ASSERT_EQ(msg_count, kMaxNumMessagesPerPacket);
}

TEST(ReliableStreamTest, ReadsDuplicateMessageOnlyOnce) {
  constexpr u32 kMessageCount = 1;
  constexpr u32 kFirstPacketId = 1;
  constexpr u32 kSecondPacketId = 2;

  ReliableStream client{};
  ReliableStream server{};

  client.send([](byte_span buffer) {
    auto count = IoBuffer(buffer).write_byte_array(span_of_string("foo"), 0);
    return buffer.first(count);
  });

  usize msg_count = 0;

  auto buffer1 = make_buffer();
  auto payload1 = client.write(kFirstPacketId, buffer1);
  server.read(kFirstPacketId, buffer1, [&msg_count](byte_span payload) {
    msg_count++;
    auto byte_array = IoBuffer(payload).read_byte_array(0, payload.size());
    ASSERT_EQ(string_of_span(byte_array), "foo");
  });

  // Tell the client that the packet has failed, so it sends the same message again.
  client.on_packet_delivery_status(kFirstPacketId, PacketDeliveryStatus::DROP);
  auto buffer2 = make_buffer();
  client.write(kSecondPacketId, buffer2);
  auto payload2 = make_buffer();
  server.read(kSecondPacketId, buffer2, [&msg_count](byte_span payload) {
    msg_count++;
    // Server shouldn't call this because it's already seen the same message.
    FAIL();
  });
  ASSERT_EQ(msg_count, 1);
}