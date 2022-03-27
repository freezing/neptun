//
// Created by freezing on 27/03/2022.
//

#ifndef NEPTUN_COMMON_FAKE_CLOCK_H
#define NEPTUN_COMMON_FAKE_CLOCK_H

#include <chrono>

#include "common/types.h"

namespace freezing {

class FakeClock {
public:
  static constexpr bool is_steady = false;

  typedef u64 rep;
  typedef std::ratio<1l, 1000000000l> period;
  typedef std::chrono::duration<rep, period> duration;
  typedef std::chrono::time_point<FakeClock> time_point;

//  static void advance(duration d) noexcept {
//    m_now = time_point();
//  }
//
//  static void set_since_epoch(duration d) noexcept {
//    m_now = time_point() + d;
//  }

  static time_point now() noexcept {
    return m_now;
  }

private:
  FakeClock() = delete;
  ~FakeClock() = delete;
  FakeClock(const FakeClock&) = delete;

  inline static time_point m_now{};
};

}

#endif //NEPTUN_COMMON_FAKE_CLOCK_H
