//
// Created by freezing on 26/03/2022.
//

#ifndef NEPTUN_COMMON_TICKER_H
#define NEPTUN_COMMON_TICKER_H

#include <chrono>

namespace freezing {

class Ticker {
public:
  explicit Ticker(std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> now,
                  std::chrono::nanoseconds tick_interval) :
      m_last_known_now{std::chrono::time_point_cast<std::chrono::nanoseconds>(now)},
      m_tick_interval{tick_interval},
      m_time_since_last_tick{tick_interval} {}

  bool tick(std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> now) {
    auto elapsed_time = (now - m_last_known_now);
    m_time_since_last_tick += elapsed_time;
    m_last_known_now = now;

    if (m_time_since_last_tick >= m_tick_interval) {
      m_time_since_last_tick -= m_tick_interval;
      return true;
    }
    return false;
  }

private:
  std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> m_last_known_now;
  std::chrono::nanoseconds m_tick_interval;
  std::chrono::nanoseconds m_time_since_last_tick;
};

}

#endif //NEPTUN_COMMON_TICKER_H
