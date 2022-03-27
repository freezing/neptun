//
// Created by freezing on 27/03/2022.
//

#ifndef NEPTUN_NEPTUN_MESSAGES_LETS_CONNECT_H
#define NEPTUN_NEPTUN_MESSAGES_LETS_CONNECT_H

#include "common/types.h"
#include "network/io_buffer.h"

namespace freezing::network {

class LetsConnect {
public:
  static constexpr u8 kId = 0;
  static constexpr usize kSerializedSize = sizeof(u8) + sizeof(u16);
  static constexpr usize kPacketRateOffset = 0;
  static constexpr usize kMaxPacketLengthOffset = kPacketRateOffset + sizeof(u8);

  static byte_span write(byte_span buffer, u8 packet_rate, u16 max_packet_length) {
    auto io = IoBuffer(buffer);
    usize count = 0;
    count += io.write_u8(packet_rate, kPacketRateOffset);
    count += io.write_u16(max_packet_length, kMaxPacketLengthOffset);
    return buffer.first(count);
  }

  explicit LetsConnect(byte_span buffer) : m_buffer{buffer} {}

  u8 packet_rate() const {
    return m_buffer.read_u8(kPacketRateOffset);
  }

  u16 max_packet_length() const {
    return m_buffer.read_u16(kMaxPacketLengthOffset);
  }

private:
  IoBuffer m_buffer;
};

}

#endif //NEPTUN_NEPTUN_MESSAGES_LETS_CONNECT_H
