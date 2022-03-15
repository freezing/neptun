//
// Created by freezing on 13/03/2022.
//

#ifndef NEPTUN_NEPTUN_MESSAGES_PACKET_H
#define NEPTUN_NEPTUN_MESSAGES_PACKET_H

#include "network/io_buffer.h"
#include "common/types.h"

namespace freezing::network {

class PacketHeader {
public:
  static constexpr usize kIdOffset = 0;
  static constexpr usize kAckSequenceNumberOffset = kIdOffset + sizeof(std::uint32_t);
  static constexpr usize kAckBitmaskOffset = kAckSequenceNumberOffset + sizeof(std::uint32_t);

  static constexpr usize kSerializedSize = sizeof(u32) + sizeof(u32) + sizeof(u32);

  static byte_span write(byte_span buffer, u32 id, u32 ack_sequence_number, u32 ack_bitmask) {
    auto io = IoBuffer(buffer);
    usize count = 0;
    count += io.write_u32(id, kIdOffset);
    count += io.write_u32(ack_sequence_number, kAckSequenceNumberOffset);
    count += io.write_u32(ack_bitmask, kAckBitmaskOffset);
    return buffer.first(count);
  }

  explicit PacketHeader(std::span<std::uint8_t> buffer) : m_buffer{buffer} {}

  u32 id() const {
    return m_buffer.read_u32(kIdOffset);
  }

  u32 ack_sequence_number() const {
    return m_buffer.read_u32(kAckSequenceNumberOffset);
  }

  u32 ack_bitmask() const {
    return m_buffer.read_u32(kAckBitmaskOffset);
  }

private:
  IoBuffer m_buffer;
};

}

#endif //NEPTUN_NEPTUN_MESSAGES_PACKET_H
