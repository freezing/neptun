//
// Created by freezing on 12/03/2022.
//

#ifndef NEPTUN__IO_BUFFER_H
#define NEPTUN__IO_BUFFER_H

#include <array>
#include <memory.h>
#include <cassert>
#include <span>
#include <string>
#include <cinttypes>
#include <netinet/in.h>

namespace freezing::network {

namespace {

// Just enough to fit the whole MTU.
constexpr int kDefaultCapacity = 1600;

}

template<int Capacity = kDefaultCapacity>
class IoBuffer {
public:
  IoBuffer() : m_buffer{} {}

  std::span<std::uint8_t> at_pos() {
    return std::span(m_buffer.begin() + m_pos, m_buffer.end());
  }

  template<typename T>
  void write_unsigned(T value) {
    auto buffer = at_pos();
    assert(sizeof(T) <= buffer.size());

    static_assert(std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint16_t>
                      || std::is_same_v<T, std::uint8_t>);

    T network;
    if constexpr (std::is_same_v<T, std::uint32_t>) {
      network = htonl(value);
    } else if constexpr (std::is_same_v<T, std::uint16_t>) {
      network = htons(value);
    } else if constexpr (std::is_same_v<T, std::uint8_t>) {
      network = value;
    }

    auto *source = reinterpret_cast<std::uint8_t *>(&network);
    auto *destination = reinterpret_cast<std::uint8_t *>(buffer.data());
    memcpy(destination, source, sizeof(T));
    m_pos += sizeof(T);
  }

  void write_u8(std::uint8_t value) {
    write_unsigned<std::uint8_t>(value);
  }

  void write_u16(std::uint16_t value) {
    write_unsigned<std::uint16_t>(value);
  }

  void write_u32(std::uint32_t value) {
    write_unsigned<std::uint32_t>(value);
  }

  void write_string(const std::string &value) {
    write_u16(value.length());
    for (int i = 0; i < value.size(); i++) {
      write_u8(value[i]);
    }
  }

  template<typename T>
  T read_unsigned() {
    auto buffer = at_pos();
    assert(sizeof(T) <= buffer.size());
    T network = *((T*) buffer.data());
    m_pos += sizeof(T);

    static_assert(std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint16_t>
                      || std::is_same_v<T, std::uint8_t>);

    if constexpr (std::is_same_v<T, std::uint32_t>) {
      return ntohl(network);
    } else if constexpr (std::is_same_v<T, std::uint16_t>) {
      return ntohs(network);
    } else if constexpr (std::is_same_v<T, std::uint8_t>) {
      return network;
    }
  }

  std::uint8_t read_u8() {
    return read_unsigned<std::uint8_t>();
  }

  std::uint16_t read_u16() {
    return read_unsigned<std::uint16_t>();
  }

  std::uint32_t read_u32() {
    return read_unsigned<std::uint32_t>();
  }

  std::string_view read_string() {
    assert(at_pos().size() >= sizeof(std::uint16_t));
    std::uint16_t length = read_u16();

    assert(length <= at_pos().size());
    auto string_buffer = at_pos();
    m_pos += length;
    return std::string_view(reinterpret_cast<char*>(string_buffer.data()), length);
  }

  void flip() {
    m_pos = 0;
  }

  void seek(int pos) {
    m_pos = pos;
  }

private:
  int m_pos{0};
  std::array<std::uint8_t, Capacity> m_buffer;

};
}

#endif //NEPTUN__IO_BUFFER_H
