//
// Created by freezing on 13/03/2022.
//

#ifndef NEPTUN_NEPTUN_MESSAGES_PACKET_H
#define NEPTUN_NEPTUN_MESSAGES_PACKET_H

#include "network/io_buffer.h"

namespace freezing::network {

class PacketHeader {
public:
  static constexpr std::size_t kIdOffset = 0;
  static constexpr std::size_t kAckSequenceNumberOffset = kIdOffset + sizeof(std::uint32_t);
  static constexpr std::size_t kAckBitmaskOffset = kAckSequenceNumberOffset + sizeof(std::uint32_t);

  static std::span<std::uint8_t> write(std::span<std::uint8_t> buffer,
                    std::uint32_t id,
                    std::uint32_t ack_sequence_number,
                    std::uint32_t ack_bitmask) {
    auto io = IoBuffer(buffer);
    std::size_t count = 0;
    count += io.write_u32(id, kIdOffset);
    count += io.write_u32(ack_sequence_number, kAckSequenceNumberOffset);
    count += io.write_u32(ack_bitmask, kAckBitmaskOffset);
    return buffer.first(count);
  }

  explicit PacketHeader(std::span<std::uint8_t> buffer) : m_buffer{buffer} {}

  std::uint32_t id() const {
    return m_buffer.read_u32(kIdOffset);
  }

  std::uint32_t ack_sequence_number() const {
    return m_buffer.read_u32(kAckSequenceNumberOffset);
  }

  std::uint32_t ack_bitmask() const {
    return m_buffer.read_u32(kAckBitmaskOffset);
  }

private:
  IoBuffer m_buffer;
};

}

#endif //NEPTUN_NEPTUN_MESSAGES_PACKET_H
