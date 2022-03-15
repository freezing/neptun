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
  }
};

struct InFlightMessage {
  u32 packet_id;
  BufferRange range;
};

class ReliableStream {
public:
  explicit ReliableStream(usize buffer_capacity = 3200) : buffer(buffer_capacity) {}

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
      std::stack<BufferRange> reversed_messages;
      while (!in_flight_messages.empty() && in_flight_messages.front().packet_id == packet_id) {
        reversed_messages.push(in_flight_messages.front().range);
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
      usize length = io.read_u16(idx);
      if (length + sizeof(u16) > buffer.size()) {
        // A bad packet. Report an error instead of logging.
        std::cout << "Bad packet: " << packet_id << std::endl;
        break;
      }
      auto byte_array = io.read_byte_array(idx + sizeof(u16), length);
      idx += sizeof(u16) + length;
      callback(byte_array);
    }
    return idx;
  }

  usize write(u16 packet_id, byte_span buffer) {
    // Figure out how many messages can we write.
    usize total_size = Segment::kSerializedSize;
    usize message_count = 0;
    for (auto buffer_range : pending_messages) {
      auto msg_span = buffer_span(buffer_range);
      usize msg_size = sizeof(u16) + msg_span.size();
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
      auto buffer_range = pending_messages.front();
      auto payload = buffer_span(buffer_range);
      pending_messages.pop_front();
      in_flight_messages.push({packet_id, buffer_range});

      usize total_message_size = payload.size() + sizeof(u16);
      assert(total_message_size <= buffer.size());

      io.write_u16(payload.size(), idx);
      io.write_byte_array(payload, idx + sizeof(u16));
      idx += total_message_size;
    }
    assert(idx == total_size);
    return total_size;
  }

  template<typename WriteToBufferFn>
  void send(WriteToBufferFn write_to_buffer) {
    flip();
    auto payload = write_to_buffer(buffer.remaining());
    if (!payload.empty()) {
      pending_messages.push_back({buffer.end_index(), buffer.end_index() + payload.size()});
      buffer.advance(payload.size());
    }
  }

private:
  FlipBuffer<u8> buffer;
  std::queue<InFlightMessage> in_flight_messages;
  std::deque<BufferRange> pending_messages;

  byte_span buffer_span(BufferRange range) {
    return {buffer.begin() + range.begin, buffer.begin() + range.end};
  }

  void flip() {
    if (buffer.begin_index() > 0) {
      // A "creative" solution:
      // Pop each element from the queue and put it back.
      // Repeat that queue.size() times, and we have effectively iterated over the elements in the
      // queue.
      for (usize i = 0; i < in_flight_messages.size(); i++) {
        auto msg = in_flight_messages.front();
        msg.range -= buffer.begin_index();
        in_flight_messages.push(msg);
      }

      for (auto& range : pending_messages) {
        range -= buffer.begin_index();
      }

      buffer.flip();
    }
  }

};

}

#endif //NEPTUN_NEPTUN_MESSAGES_RELIABLE_STREAM_H
