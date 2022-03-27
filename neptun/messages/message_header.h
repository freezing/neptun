//
// Created by freezing on 13/03/2022.
//

#ifndef NEPTUN_NEPTUN_MESSAGES_MESSAGE_HEADER_H
#define NEPTUN_NEPTUN_MESSAGES_MESSAGE_HEADER_H

#include "common/types.h"
#include "network/io_buffer.h"

// TODO: Move every message to freezing::network::message namespace.
// This would make it easier to read messages in code, e.g. message::LetsConnect is more readable
// than just LetsConnect because it provides additional context that LetsConnect is a message.
namespace freezing::network {

class MessageHeader {
public:
  static constexpr usize kMessageTypeOffset = 0;

  static constexpr usize kSerializedSize = sizeof(u8);

  static byte_span write(byte_span buffer, u8 message_type) {
    auto io = IoBuffer(buffer);
    usize count = io.write_u8(message_type, kMessageTypeOffset);
    return buffer.first(count);
  }

  explicit MessageHeader(byte_span buffer) : m_buffer{buffer} {}

  u8 message_type() const {
    return m_buffer.read_u8(kMessageTypeOffset);
  }

private:
  IoBuffer m_buffer;
};

}

#endif //NEPTUN_NEPTUN_MESSAGES_MESSAGE_HEADER_H
