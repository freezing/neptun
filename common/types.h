//
// Created by freezing on 13/03/2022.
//

#ifndef NEPTUN_COMMON_COMMON_H
#define NEPTUN_COMMON_COMMON_H

#include <chrono>
#include <cinttypes>
#include <span>
#include <tl/expected.hpp>

namespace freezing {

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = std::size_t;
using byte_span = std::span<u8>;
// TODO: This should be [const u8], but I have to split [IoBuffer] first.
using const_byte_span = std::span<u8>;
// TODO: Introduce [time_ns] to avoid precision bugs.

using seconds = std::chrono::seconds;
using milliseconds = std::chrono::milliseconds;
using microseconds = std::chrono::microseconds;
using nanoseconds = std::chrono::nanoseconds;

template<typename Clock>
using time_point = std::chrono::time_point<Clock, nanoseconds>;

using system_clock = std::chrono::system_clock;

inline byte_span advance(byte_span span, usize count) {
  return span.last(span.size() - count);
}

template<class T, class E>
using expected = tl::expected<T, E>;

template<class E>
using Error = tl::unexpected<E>;

template<typename E>
Error<E> make_error(E&& e) {
  return Error<typename std::decay<E>::type>(std::forward<E>(e));
}

}

#endif //NEPTUN_COMMON_COMMON_H
