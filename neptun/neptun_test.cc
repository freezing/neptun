#include <gtest/gtest.h>

#include "network/fake_network.h"
#include "neptun/neptun.h"

using namespace freezing;
using namespace freezing::network;
using namespace freezing::network::testing;

namespace {
static const IpAddress kServerIp = IpAddress::from_ipv4("192.168.0.10", 12345);
static const IpAddress kClientIp = IpAddress::from_ipv4("192.168.0.11", 12345);
// 2022-03-15 21:18:41 GMT (not relevant, but doesn't hurt to know).
constexpr u64 kNow = 1647379116;

const auto unexpected_reliable_msgs = [](byte_span buffer) { FAIL(); };
}

TEST(NeptunTest, ReadAndWriteSingleReliableMessage) {
  FakeNetwork fake_network{};
  Neptun server{fake_network, kServerIp};
  Neptun client{fake_network, kClientIp};

  client.send_reliable_to(kServerIp, [](byte_span buffer) {
    IoBuffer io{buffer};
    auto count = io.write_string("this is test string", 0);
    return buffer.first(count);
  });
  client.tick(kNow, unexpected_reliable_msgs);

  usize msg_count = 0;
  server.tick(kNow, [&msg_count](byte_span payload) {
    IoBuffer io{payload};
    ASSERT_EQ(io.read_string(0), "this is test string");
    msg_count++;
  });
  ASSERT_EQ(msg_count, 1);
}

TEST(NeptunTest, ReadAndWriteSingleReliableMessage_DropPackets) {
  FakeNetwork fake_network{};
  Neptun server{fake_network, kServerIp};
  Neptun client{fake_network, kClientIp};

  client.send_reliable_to(kServerIp, [](byte_span buffer) {
    IoBuffer io{buffer};
    auto count = io.write_string("this is test string", 0);
    return buffer.first(count);
  });
  fake_network.drop_packets(true);
  client.tick(kNow, unexpected_reliable_msgs);

  usize reliable_msg_count = 0;
  server.tick(kNow, [&reliable_msg_count](byte_span payload) {
    reliable_msg_count++;
  });
  // Server never received any packets.
  ASSERT_EQ(reliable_msg_count, 0);

  fake_network.drop_packets(false);
  // We don't expect any reliable messages to be sent anymore because the client doesn't know
  // that the packets are dropped.
  client.tick(kNow, unexpected_reliable_msgs);
  reliable_msg_count = 0;
  server.tick(kNow, [&reliable_msg_count](byte_span payload) {
    reliable_msg_count++;
  });
  ASSERT_EQ(reliable_msg_count, 0);

  // Server responds to client with acks, but no reliable messages.
  // The acks tell the client that previously sent reliable messages haven't been delivered.
  server.tick(kNow, unexpected_reliable_msgs);

  reliable_msg_count = 0;
  client.tick(kNow, [&reliable_msg_count](byte_span payload) {
    reliable_msg_count++;
  });
  ASSERT_EQ(reliable_msg_count, 0);

  // The client resends the reliable message because it was dropped.
  client.tick(kNow, unexpected_reliable_msgs);

  reliable_msg_count = 0;
  server.tick(kNow, [&reliable_msg_count](byte_span payload) {
    IoBuffer io{payload};
    ASSERT_EQ(io.read_string(0), "this is test string");
    reliable_msg_count++;
  });
  ASSERT_EQ(reliable_msg_count, 1);
}

TEST(NeptunTest, ReadAndWriteMultipleReliableMessage) {
  FakeNetwork fake_network{};
  Neptun server{fake_network, kServerIp};
  Neptun client{fake_network, kClientIp};

  constexpr usize kNumMessages = 10;

  for (usize i = 0; i < kNumMessages; i++) {
    client.send_reliable_to(kServerIp, [i](byte_span buffer) {
      IoBuffer io{buffer};
      auto count = io.write_string("this is test string " + std::to_string(i), 0);
      return buffer.first(count);
    });
  }
  client.tick(kNow, unexpected_reliable_msgs);

  usize msg_count = 0;
  server.tick(kNow, [&msg_count](byte_span payload) {
    IoBuffer io{payload};
    ASSERT_EQ(io.read_string(0), "this is test string " + std::to_string(msg_count));
    msg_count++;
  });
  ASSERT_EQ(msg_count, 10);
}

TEST(NeptunTest, Timeouts) {
  constexpr u32 kPacketTimeoutSeconds = 5;

  FakeNetwork fake_network{};
  Neptun server{fake_network, kServerIp};
  Neptun client{fake_network, kClientIp, kPacketTimeoutSeconds};

  client.send_reliable_to(kServerIp, [](byte_span buffer) {
    IoBuffer io{buffer};
    auto count = io.write_string("this is test string", 0);
    return buffer.first(count);
  });
  fake_network.drop_packets(true);
  client.tick(kNow, unexpected_reliable_msgs);
  server.tick(kNow, unexpected_reliable_msgs);

  fake_network.drop_packets(false);
  client.tick(kNow + 6, unexpected_reliable_msgs);
  usize msg_count = 0;
  server.tick(kNow, [&msg_count](byte_span payload) {
    IoBuffer io{payload};
    ASSERT_EQ(io.read_string(0), "this is test string");
    msg_count++;
  });
  ASSERT_EQ(msg_count, 1);
}

TEST(NeptunTest, ReliableMessageAfterDroppingMultiplePackets) {
  FakeNetwork fake_network{};
  Neptun server{fake_network, kServerIp};
  Neptun client{fake_network, kClientIp};

  auto send_to_server = [&client](usize sequence_number) {
    client.send_reliable_to(kServerIp, [sequence_number](byte_span buffer) {
      IoBuffer io{buffer};
      auto count = io.write_string("this is test string " + std::to_string(sequence_number), 0);
      return buffer.first(count);
    });
  };

  // Drop first 5 packets.
  fake_network.drop_packets(true);
  for (usize sequence_number = 0; sequence_number < 5; sequence_number++) {
    send_to_server(sequence_number);
    client.tick(kNow, unexpected_reliable_msgs);
  }

  // Add one more reliable message.
  // The packets are received on the server-side.
  fake_network.drop_packets(false);
  send_to_server(5);
  client.tick(kNow, unexpected_reliable_msgs);
  // Server doesn't receive any reliable messages yet, because the last one that succeeded
  // is dropped due to not being sent in order.
  server.tick(kNow, unexpected_reliable_msgs);

  // Client receives acks that the first 5 packets are dropped.
  // It sends all messages to the server again.
  client.tick(kNow, unexpected_reliable_msgs);

  usize msg_count = 0;
  server.tick(kNow, [&msg_count](byte_span buffer) {
    IoBuffer io{buffer};
    ASSERT_EQ(io.read_string(0), "this is test string " + std::to_string(msg_count));
    msg_count++;
  });
  ASSERT_EQ(msg_count, 6);
}

TEST(NeptunTest, IgnoresPacketsForUnrelatedProtocol) {
  FAIL();
}