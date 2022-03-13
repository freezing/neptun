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
  static constexpr std::size_t kNoteOffset = kTimestampOffset + sizeof(std::uint64_t);

  static void write(IoBuffer<C> &buffer, std::uint64_t timestamp, const std::string &note) {
    buffer.write_u64(timestamp);
    buffer.write_string(note);
  }

  explicit Ping(IoBuffer<C>& buffer) : m_buffer{buffer} {}

  std::uint64_t timestamp() const {
    // TODO: setting upper_bound doesn't make sense here.
    m_buffer.seek(kTimestampOffset, 0);
    return m_buffer.read_u32();
  }

  std::string_view note() const {
    m_buffer.seek(kNoteOffset, 0);
    return m_buffer.read_string();
  }

private:
  IoBuffer<C>& m_buffer;
};

}

#endif //NEPTUN__MESSAGE_H
