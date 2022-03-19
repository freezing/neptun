//
// Created by freezing on 19/03/2022.
//

#ifndef NEPTUN_NEPTUN_MESSAGES_UNRELIABLE_MESSAGE_H
#define NEPTUN_NEPTUN_MESSAGES_UNRELIABLE_MESSAGE_H

#include "network/io_buffer.h"
#include "common/types.h"
#include "common/errors.h"

namespace freezing::network {

class UnreliableMessage {
public:
  static constexpr usize kLengthOffset = 0;
  static constexpr usize kPayloadOffset = kLengthOffset + sizeof(u16);

  static constexpr usize serialized_size(usize payload_size) {
    return sizeof(u16) + payload_size;
  }

  static byte_span write(byte_span buffer, u16 length, byte_span payload) {
    auto io = IoBuffer(buffer);
    usize count = 0;
    count += io.write_u16(length, kLengthOffset);
    count += io.write_byte_array(payload, kPayloadOffset);
    return buffer.first(count);
  }

  explicit UnreliableMessage(byte_span buffer) : m_buffer{buffer} {}

  u16 length() const {
    return m_buffer.read_u16(kLengthOffset);
  }

  byte_span payload() const {
    return m_buffer.read_byte_array(kPayloadOffset, length());
  }

  usize serialized_size() const {
    return sizeof(u16) + m_buffer.read_u16(kLengthOffset);
  }

  expected<usize, EncodingError> validate_size() const {
    usize minimum_size = sizeof(u16);
    if (minimum_size > m_buffer.size()) {
      return make_error(EncodingError::MALFORMED_BUFFER);
    }
    usize total_size = minimum_size + length();
    // UnreliableMessage must not have length=0 because it makes no sense to waste bandwidth
    // on an empty message.
    if (total_size > m_buffer.size() || total_size == minimum_size) {
      return make_error(EncodingError::MALFORMED_BUFFER);
    }
    return {total_size};
  }

private:
  IoBuffer m_buffer;
};

}

#endif //NEPTUN_NEPTUN_MESSAGES_UNRELIABLE_MESSAGE_H
