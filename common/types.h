//
// Created by freezing on 13/03/2022.
//

#ifndef NEPTUN_COMMON_COMMON_H
#define NEPTUN_COMMON_COMMON_H

#include <cinttypes>
#include <span>

namespace freezing {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = std::size_t;
using byte_span = std::span<u8>;

inline byte_span advance(byte_span span, usize count) {
  return span.last(span.size() - count);
}

}

#endif //NEPTUN_COMMON_COMMON_H
