//
// Created by freezing on 19/03/2022.
//

#ifndef NEPTUN_NEPTUN_MESSAGES_RELIABLE_MESSAGE_H
#define NEPTUN_NEPTUN_MESSAGES_RELIABLE_MESSAGE_H

#include "network/io_buffer.h"
#include "common/types.h"

namespace freezing::network {

class ReliableMessage {
public:
  static constexpr usize kSequenceNumberOffset = 0;
  static constexpr usize kLengthOffset = kSequenceNumberOffset + sizeof(u32);
  static constexpr usize kPayloadOffset = kLengthOffset + sizeof(u16);

  static constexpr usize serialized_size(usize payload_size) {
    return sizeof(u32) + sizeof(u16) + payload_size;
  }

  static byte_span write(byte_span buffer, u32 sequence_number, u16 length, byte_span payload) {
    auto io = IoBuffer(buffer);
    usize count = 0;
    count += io.write_u32(sequence_number, kSequenceNumberOffset);
    count += io.write_u16(length, kLengthOffset);
    count += io.write_byte_array(payload, kPayloadOffset);
    return buffer.first(count);
  }

  explicit ReliableMessage(byte_span buffer) : m_buffer{buffer} {}

  u32 sequence_number() const {
    return m_buffer.read_u32(kSequenceNumberOffset);
  }

  u16 length() const {
    return m_buffer.read_u16(kLengthOffset);
  }

  byte_span payload() const {
    return m_buffer.read_byte_array(kPayloadOffset, length());
  }

  usize serialized_size() const {
    return sizeof(u32) + sizeof(u16) + m_buffer.read_u16(kLengthOffset);
  }

private:
  IoBuffer m_buffer;
};

}

#endif //NEPTUN_NEPTUN_MESSAGES_RELIABLE_MESSAGE_H
