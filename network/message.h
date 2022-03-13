//
// Created by freezing on 12/03/2022.
//

#ifndef NEPTUN__MESSAGE_H
#define NEPTUN__MESSAGE_H

#include <array>
#include <memory.h>
#include <cassert>
#include <span>
#include <string>
#include <cinttypes>
#include <netinet/in.h>

#include "io_buffer.h"

namespace freezing::network {

// TODO: Move magic numbers to constants.h
class Ping {
public:
  static constexpr std::size_t kTimestampOffset = 0;
  static constexpr std::size_t kNoteOffset = kTimestampOffset + sizeof(std::uint64_t);

  static std::span<std::uint8_t> write(std::span<std::uint8_t> buffer, std::uint64_t timestamp, const std::string &note) {
    auto io = IoBuffer(buffer);
    std::size_t byte_count = 0;
    byte_count += io.write_u64(timestamp, kTimestampOffset);
    byte_count += io.write_string(note, kNoteOffset);
    return buffer.first(byte_count);
  }

  explicit Ping(std::span<std::uint8_t> buffer) : m_buffer{buffer} {}

  std::uint64_t timestamp() const {
    return m_buffer.read_u64(kTimestampOffset);
  }

  std::string_view note() const {
    return m_buffer.read_string(kNoteOffset);
  }

private:
  IoBuffer m_buffer;
};

}

#endif //NEPTUN__MESSAGE_H
