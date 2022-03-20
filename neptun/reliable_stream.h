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
#include "neptun/error.h"
#include "neptun/messages/message_header.h"
#include "neptun/messages/segment.h"
#include "neptun/messages/reliable_message.h"

namespace freezing::network {

struct BufferRange {
  usize begin;
  usize end;

  BufferRange &operator-=(usize value) {
    assert(begin >= value);
    assert(end >= value);
    begin -= value;
    end -= value;
    return *this;
  }

  usize size() const {
    assert(begin <= end);
    return end - begin;
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

// TODO: Can handle dropped messages more efficiently, e.g. by having a window of reliable messages
// that are acked and only resending the ones that are dropped.
// This is required on the sender and on the receiver side.
class ReliableStream {
public:
  explicit ReliableStream(usize buffer_capacity = 3200) : m_buffer(buffer_capacity) {}

  void on_packet_delivery_status(u32 packet_id, PacketDeliveryStatus status) {
    switch (status) {
    case PacketDeliveryStatus::ACK:
      while (!in_flight_messages.empty() && in_flight_messages.front().packet_id == packet_id) {
        m_buffer.consume(in_flight_messages.front().message.range.size());
        in_flight_messages.pop();
      }
      assert(in_flight_messages.empty() || in_flight_messages.front().packet_id > packet_id);
      break;
    case PacketDeliveryStatus::DROP:
      assert(in_flight_messages.empty() || in_flight_messages.front().packet_id >= packet_id);
      std::stack<PendingMessage> reversed_messages;
      while (!in_flight_messages.empty()) {
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
  expected<usize, NeptunError> read(u16 packet_id, byte_span buffer, ReliableMessageCallback callback) {
    if (Segment::kSerializedSize > buffer.size()) {
      // No segment to read.
      return 0;
    }

    auto segment = Segment(buffer);
    if (segment.manager_type() != ManagerType::RELIABLE_STREAM) {
      // The segment is for another manager.
      // This probably means that the ReliableStream segment doesn't exist.
      return 0;
    }
    usize idx = Segment::kSerializedSize;
    for (int i = 0; i < segment.message_count(); i++) {
      // TODO: Advance buffer and then [buffer.size() - idx] is ugly. Refactor.
      // The same in other places.
      // TODO: What if buffer is malformed. I need a better mechanism to deal with this in a
      // generic way. E.g. maybe ReliableMessage (and similar msgs) should only provide
      // validate_size API?
      ReliableMessage reliable_message(advance(buffer, idx));
      const auto msg_size = reliable_message.validate_size();
      if (!msg_size) {
        // A packet is malformed.
        return make_error(NeptunError::MALFORMED_PACKET);
      }
      idx += *msg_size;

      // It's important that we process all messages so that the buffer pointer is updated
      // correctly.
      if (reliable_message.sequence_number() == m_next_expected_sequence_number) {
        m_next_expected_sequence_number++;
        callback(reliable_message.payload());
      }
    }
    return idx;
  }

  usize write(u32 packet_id, byte_span buffer) {
    // Figure out how many messages can we write.
    usize total_size = Segment::kSerializedSize;
    usize message_count = 0;
    for (auto pending_msg : pending_messages) {
      auto payload = buffer_span(pending_msg.range);
      const usize msg_size = ReliableMessage::serialized_size(payload.size());
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
      in_flight_messages.push({packet_id, pending_msg});

      auto reliable_message_buffer = ReliableMessage::write(advance(buffer, idx),
                             pending_msg.sequence_number,
                             payload.size(),
                             payload);

      usize total_message_size = reliable_message_buffer.size();
      assert(total_message_size <= buffer.size() - idx);
      idx += total_message_size;
    }
    assert(idx == total_size);
    return total_size;
  }

  template<typename WriteToBufferFn>
  void send(WriteToBufferFn write_to_buffer) {
    maybe_flip();
    // TODO: It's a nicer API for the user if [write_to_buffer] returns [usize].
    auto payload = write_to_buffer(m_buffer.remaining());
    if (!payload.empty()) {
      auto sequence_number = m_next_outgoing_sequence_number++;
      pending_messages.push_back({m_buffer.end_index(), m_buffer.end_index() + payload.size(),
                                  sequence_number});
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

  void maybe_flip() {
    if (m_buffer.begin_index() > 0) {
      // A "creative" solution:
      // Pop each element from the queue and put it back.
      // Repeat that queue.size() times, and we have effectively iterated over the elements in the
      // queue.
      for (usize i = 0; i < in_flight_messages.size(); i++) {
        auto in_flight_msg = in_flight_messages.front();
        in_flight_messages.pop();
        in_flight_msg.message.range -= m_buffer.begin_index();
        in_flight_messages.push(in_flight_msg);
      }

      for (auto &pending_msg : pending_messages) {
        pending_msg.range -= m_buffer.begin_index();
      }

      m_buffer.flip();
    }
  }

};

}

#endif //NEPTUN_NEPTUN_MESSAGES_RELIABLE_STREAM_H
