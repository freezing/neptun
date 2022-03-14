//
// Created by freezing on 13/03/2022.
//

#ifndef NEPTUN_NEPTUN_MESSAGES_MESSAGE_HEADER_H
#define NEPTUN_NEPTUN_MESSAGES_MESSAGE_HEADER_H

#include "common/types.h"
#include "network/io_buffer.h"

namespace freezing::network {

enum MessageType {

};

class MessageHeader {
public:
  static constexpr usize kMessageTypeOffset = 0;

  static constexpr usize kSerializedSize = sizeof(u16);

  static byte_span write(byte_span buffer, u16 message_type) {
    auto io = IoBuffer(buffer);
    usize count = io.write_u16(message_type, kMessageTypeOffset);
    return buffer.first(count);
  }

  explicit MessageHeader(byte_span buffer) : m_buffer{buffer} {}

  u16 message_type() const {
    return m_buffer.read_u16(kMessageTypeOffset);
  }

private:
  IoBuffer m_buffer;
};

}

#endif //NEPTUN_NEPTUN_MESSAGES_MESSAGE_HEADER_H
