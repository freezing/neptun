//
// Created by freezing on 12/03/2022.
//

#ifndef NEPTUN__IO_BUFFER_H
#define NEPTUN__IO_BUFFER_H

#include <array>
#include <memory.h>
#include <cassert>
#include <cmath>
#include <span>
#include <string>
#include <cinttypes>
#include <netinet/in.h>

#include "common/types.h"

namespace freezing::network {

class IoBuffer {
public:
  IoBuffer(std::span<std::uint8_t> buffer) : m_buffer{buffer} {}

  template<typename T>
  std::size_t write_unsigned(T value, usize idx) {
    static_assert(std::is_same_v<T, std::uint64_t> || std::is_same_v<T, std::uint32_t>
                      || std::is_same_v<T, std::uint16_t>
                      || std::is_same_v<T, std::uint8_t>);

    // See: https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html
    for (int i = 0; i < sizeof(T); i++) {
      T shift = 8 * (sizeof(T) - 1 - i);
      m_buffer[idx + i] = static_cast<std::uint8_t>((value >> shift) & 0xFF);
    }
    return sizeof(T);
  }

  std::size_t write_u8(std::uint8_t value, usize idx) {
    return write_unsigned<std::uint8_t>(value, idx);
  }

  std::size_t write_u16(std::uint16_t value, usize idx) {
    return write_unsigned<std::uint16_t>(value, idx);
  }

  std::size_t write_u32(std::uint32_t value, usize idx) {
    return write_unsigned<std::uint32_t>(value, idx);
  }

  std::size_t write_u64(std::uint64_t value, usize idx) {
    return write_unsigned<std::uint64_t>(value, idx);
  }

  std::size_t write_string(const std::string &value, usize idx) {
    write_u16(value.length(), idx);
    for (int i = 0; i < value.size(); i++) {
      write_u8(value[i], idx + sizeof(std::uint16_t) + i);
    }
    return sizeof(std::uint16_t) + sizeof(std::uint8_t) * value.size();
  }

  usize write_byte_array(byte_span data, usize idx) {
    // TODO: This can probably be optimized with memcpy?
    for (u8 byte : data) {
      m_buffer[idx++] = byte;
    }
    return data.size();
  }

  template<typename T>
  T read_unsigned(usize idx) const {
    static_assert(std::is_same_v<T, std::uint64_t> || std::is_same_v<T, std::uint32_t>
                      || std::is_same_v<T, std::uint16_t>
                      || std::is_same_v<T, std::uint8_t>);
    T value = 0;
    for (int i = 0; i < sizeof(T); i++) {
      T byte = m_buffer[idx + i];
      T shift = 8 * (sizeof(T) - 1 - i);
      value = value | (byte << shift);
    }
    return value;
  }

  std::uint8_t read_u8(usize idx) const {
    return read_unsigned<std::uint8_t>(idx);
  }

  std::uint16_t read_u16(usize idx) const {
    return read_unsigned<std::uint16_t>(idx);
  }

  std::uint32_t read_u32(usize idx) const {
    return read_unsigned<std::uint32_t>(idx);
  }

  std::uint32_t read_u64(usize idx) const {
    return read_unsigned<std::uint64_t>(idx);
  }

  std::string_view read_string(usize idx) const {
    std::uint16_t length = read_u16(idx);
    return std::string_view(reinterpret_cast<char *>(m_buffer.data() + idx + sizeof(length)),
                            length);
  }

  std::span<std::uint8_t> read_byte_array(usize idx, std::size_t length) const {
    // TODO: Is this the best place to cutoff the packet?
    // Maybe it's better to detect malicious buffers at the higher level instead of cutting them
    // off. This doesn't provide additional info that the packet is corrupt.
    usize max_length = std::min(m_buffer.size() - idx, length);
    return m_buffer.subspan(idx, max_length);
  }

private:
  std::span<std::uint8_t> m_buffer;

};
}

#endif //NEPTUN__IO_BUFFER_H
