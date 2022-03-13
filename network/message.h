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
template<int C = 1600>
class Ping {
public:
  static constexpr std::size_t kTimestampOffset = 0;
  static constexpr std::size_t kNoteOffset = kTimestampOffset + sizeof(std::uint32_t);

  static void write(IoBuffer<C> &buffer, std::uint32_t timestamp, const std::string &note) {
    buffer.write_u32(timestamp);
    buffer.write_string(note);
  }

  explicit Ping(IoBuffer<C>& buffer) : m_buffer{buffer} {}

  std::uint32_t timestamp() const {
    m_buffer.seek(kTimestampOffset);
    return m_buffer.read_u32();
  }

  std::string_view note() const {
    m_buffer.seek(kNoteOffset);
    return m_buffer.read_string();
  }

private:
  IoBuffer<C>& m_buffer;
};

}

#endif //NEPTUN__MESSAGE_H
