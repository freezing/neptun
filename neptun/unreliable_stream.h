//
// Created by freezing on 19/03/2022.
//

#ifndef NEPTUN_NEPTUN_UNRELIABLE_STREAM_H
#define NEPTUN_NEPTUN_UNRELIABLE_STREAM_H

#include <queue>

#include "common/types.h"
#include "common/flip_buffer.h"
#include "neptun/messages/segment.h"
#include "neptun/messages/unreliable_message.h"
#include "error.h"

namespace freezing::network {

class UnreliableStream {
public:
  explicit UnreliableStream(usize buffer_capacity = 3200) : m_buffer(buffer_capacity) {}

  template<typename UnreliableMessageCallback>
  expected<usize, NeptunError> read(byte_span buffer, UnreliableMessageCallback callback) {
    if (Segment::kSerializedSize > buffer.size()) {
      // No segment to read.
      return 0;
    }

    auto segment = Segment(buffer);
    if (segment.manager_type() != ManagerType::UNRELIABLE_STREAM) {
      // The segment is for another manager.
      // This probably means that the UnreliableStream segment doesn't exist.
      return 0;
    }
    usize idx = Segment::kSerializedSize;
    for (int i = 0; i < segment.message_count(); i++) {
      UnreliableMessage unreliable_message(advance(buffer, idx));
      const auto msg_size = unreliable_message.validate_size();
      if (!msg_size) {
        // A packet is malformed.
        return make_error(NeptunError::MALFORMED_PACKET);
      }
      idx += *msg_size;
      callback(unreliable_message.payload());
    }
    return idx;
  }

  usize write(byte_span buffer) {
    // Figure out how many messages can we write.
    usize total_size = Segment::kSerializedSize;
    usize message_count = 0;
    for (auto payload : m_pending_messages) {
      const usize msg_size = UnreliableMessage::serialized_size(payload.size());
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

    auto segment = Segment::write(buffer, ManagerType::UNRELIABLE_STREAM, message_count);

    usize idx = segment.size();
    auto io = IoBuffer{buffer};
    for (usize i = 0; i < message_count; i++) {
      auto payload = m_pending_messages.front();
      m_pending_messages.pop_front();
      m_buffer.consume(payload.size());
      auto unreliable_message_buffer = UnreliableMessage::write(advance(buffer, idx),
                                                                payload.size(),
                                                                payload);
      usize total_message_size = unreliable_message_buffer.size();
      assert(total_message_size <= buffer.size() - idx);
      idx += total_message_size;
    }
    assert(idx == total_size);
    // Drop all pending messages if we couldn't send them in one packet.
    m_buffer.flip();
    m_pending_messages.clear();
    return total_size;
  }

  template<typename WriteToBufferFn>
  void send(WriteToBufferFn write_to_buffer) {
    auto payload = write_to_buffer(m_buffer.remaining());
    if (!payload.empty()) {
      m_pending_messages.push_back(payload);
      m_buffer.advance(payload.size());
    }
  }

private:
  FlipBuffer<u8> m_buffer;
  std::deque<byte_span> m_pending_messages;
};

}

#endif //NEPTUN_NEPTUN_UNRELIABLE_STREAM_H
