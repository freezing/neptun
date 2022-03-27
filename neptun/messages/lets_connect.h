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
  static constexpr usize kSerializedSize = sizeof(u8) + sizeof(u8) + sizeof(u16) + sizeof(u16);
  static constexpr usize kMaxSendPacketRate = 0;
  static constexpr usize kMaxReadPacketRate = kMaxSendPacketRate + sizeof(u8);
  static constexpr usize kMaxSendPacketSize = kMaxReadPacketRate + sizeof(u16);
  static constexpr usize kMaxReadPacketSize = kMaxSendPacketSize + sizeof(u16);

  static byte_span write(byte_span buffer,
                         u8 max_send_packet_rate,
                         u8 max_read_packet_rate,
                         u16 max_send_packet_size,
                         u16 max_read_packet_size) {
    auto io = IoBuffer(buffer);
    usize count = 0;
    count += io.write_u8(max_send_packet_rate, kMaxSendPacketRate);
    count += io.write_u8(max_read_packet_rate, kMaxReadPacketRate);
    count += io.write_u16(max_send_packet_size, kMaxSendPacketSize);
    count += io.write_u16(max_read_packet_size, kMaxReadPacketSize);
    return buffer.first(count);
  }

  explicit LetsConnect(byte_span buffer) : m_buffer{buffer} {}

  u8 max_send_packet_rate() const {
    return m_buffer.read_u8(kMaxSendPacketRate);
  }

  u8 max_read_packet_rate() const {
    return m_buffer.read_u8(kMaxReadPacketRate);
  }

  u16 max_send_packet_size() const {
    return m_buffer.read_u16(kMaxSendPacketSize);
  }

  u16 max_read_packet_size() const {
    return m_buffer.read_u16(kMaxReadPacketSize);
  }

private:
  IoBuffer m_buffer;
};

}

#endif //NEPTUN_NEPTUN_MESSAGES_LETS_CONNECT_H
