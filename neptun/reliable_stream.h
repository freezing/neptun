//
// Created by freezing on 14/03/2022.
//

#ifndef NEPTUN_NEPTUN_MESSAGES_RELIABLE_STREAM_H
#define NEPTUN_NEPTUN_MESSAGES_RELIABLE_STREAM_H

#include <map>
#include <stack>
#include <queue>
#include <vector>

#include "common/types.h"
#include "common/flip_buffer.h"
#include "neptun/common.h"
#include "network/network.h"
#include "network/udp_socket.h"
#include "neptun/messages/message_header.h"
#include "neptun/messages/segment.h"

namespace freezing::network {

struct BufferRange {
  usize begin;
  usize end;

  BufferRange& operator -= (usize value) {
    assert(begin >= value);
    assert(end >= value);
    begin -= value;
    end -= value;
    return *this;
  }
};

struct PendingMessage {
  BufferRange range;
  u32 sequence_number;
};

struct InFlightMessage {
  u32 packet_id;
  PendingMessage message;
};

class ReliableStream {
public:
  explicit ReliableStream(usize buffer_capacity = 3200) : m_buffer(buffer_capacity) {}

  void on_packet_delivery_status(u32 packet_id, PacketDeliveryStatus status) {
    switch (status) {
    case PacketDeliveryStatus::ACK:
      while (!in_flight_messages.empty() && in_flight_messages.front().packet_id == packet_id) {
        in_flight_messages.pop();
      }
      assert(in_flight_messages.empty() || in_flight_messages.front().packet_id > packet_id);
      break;
    case PacketDeliveryStatus::DROP:assert(
          in_flight_messages.empty() || in_flight_messages.front().packet_id >= packet_id);
      std::stack<PendingMessage> reversed_messages;
      while (!in_flight_messages.empty() && in_flight_messages.front().packet_id == packet_id) {
        reversed_messages.push(in_flight_messages.front().message);
        in_flight_messages.pop();
      }
      while (!reversed_messages.empty()) {
        pending_messages.push_front(reversed_messages.top());
        reversed_messages.pop();
      }
      break;
    }
  }

  template<typename ReliableMessageCallback>
  // TODO: Instead of a callback, maybe return a list of spans that represent reliable messages?
  usize read(u16 packet_id, byte_span buffer, ReliableMessageCallback callback) {
    if (Segment::kSerializedSize > buffer.size()) {
      return 0;
    }

    auto segment = Segment(buffer);
    if (segment.manager_type() != ManagerType::RELIABLE_STREAM) {
      return 0;
    }
    usize idx = Segment::kSerializedSize;
    for (int i = 0; i < segment.message_count(); i++) {
      auto io = IoBuffer{buffer};
      // TODO: Read ReliableMessage.
      u32 sequence_number = io.read_u32(idx);
      usize length = io.read_u16(idx + sizeof(u32));
      if (sizeof(u32) + length + sizeof(u16) > buffer.size()) {
        // A bad packet. Report an error instead of logging.
        std::cout << "Bad packet: " << packet_id << std::endl;
        break;
      }
      auto byte_array = io.read_byte_array(idx + sizeof(u32) + sizeof(u16), length);
      idx += sizeof(u32) + sizeof(u16) + length;

      // It's important that we process all messages so that the buffer pointer is updated
      // correctly.
      if (sequence_number == m_next_expected_sequence_number) {
        m_next_expected_sequence_number++;
        callback(byte_array);
      }
    }
    return idx;
  }

  usize write(u16 packet_id, byte_span buffer) {
    // Figure out how many messages can we write.
    usize total_size = Segment::kSerializedSize;
    usize message_count = 0;
    for (auto pending_msg : pending_messages) {
      auto msg_span = buffer_span(pending_msg.range);
      // TODO: Get serialized size for ReliableMessage.
      usize msg_size = sizeof(u32) + sizeof(u16) + msg_span.size();
      if (total_size + msg_size > buffer.size()) {
        break;
      }
      total_size += msg_size;
      message_count++;
    }

    if (message_count == 0) {
      // No point in serializing anything.
      return 0;
    }

    auto segment = Segment::write(buffer, ManagerType::RELIABLE_STREAM, message_count);

    usize idx = segment.size();
    auto io = IoBuffer{buffer};
    for (usize i = 0; i < message_count; i++) {
      auto pending_msg = pending_messages.front();
      auto payload = buffer_span(pending_msg.range);
      pending_messages.pop_front();
      in_flight_messages.push({packet_id, pending_msg.range});

      usize total_message_size = sizeof(u32) + payload.size() + sizeof(u16);
      assert(total_message_size <= buffer.size());

      // TODO: Extract this into ReliableMessage.
      io.write_u32(pending_msg.sequence_number, idx);
      io.write_u16(payload.size(), idx + sizeof(u32));
      io.write_byte_array(payload, idx + sizeof(u32) + sizeof(u16));
      idx += total_message_size;
    }
    assert(idx == total_size);
    return total_size;
  }

  template<typename WriteToBufferFn>
  void send(WriteToBufferFn write_to_buffer) {
    flip();
    // TODO: It's a nicer API for the user if [write_to_buffer] returns [usize].
    auto payload = write_to_buffer(m_buffer.remaining());
    if (!payload.empty()) {
      auto sequence_number = m_next_outgoing_sequence_number++;
      pending_messages.push_back({m_buffer.end_index(), m_buffer.end_index() + payload.size(), sequence_number});
      m_buffer.advance(payload.size());
    }
  }

private:
  FlipBuffer<u8> m_buffer;
  std::queue<InFlightMessage> in_flight_messages;
  std::deque<PendingMessage> pending_messages;
  u32 m_next_outgoing_sequence_number{0};
  u32 m_next_expected_sequence_number{0};

  byte_span buffer_span(BufferRange range) {
    return {m_buffer.begin() + range.begin, m_buffer.begin() + range.end};
  }

  void flip() {
    if (m_buffer.begin_index() > 0) {
      // A "creative" solution:
      // Pop each element from the queue and put it back.
      // Repeat that queue.size() times, and we have effectively iterated over the elements in the
      // queue.
      for (usize i = 0; i < in_flight_messages.size(); i++) {
        auto in_flight_msg = in_flight_messages.front();
        in_flight_msg.message.range -= m_buffer.begin_index();
        in_flight_messages.push(in_flight_msg);
      }

      for (auto& pending_msg : pending_messages) {
        pending_msg.range -= m_buffer.begin_index();
      }

      m_buffer.flip();
    }
  }

};

}

#endif //NEPTUN_NEPTUN_MESSAGES_RELIABLE_STREAM_H
