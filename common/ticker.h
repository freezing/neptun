//
// Created by freezing on 26/03/2022.
//

#ifndef NEPTUN_COMMON_TICKER_H
#define NEPTUN_COMMON_TICKER_H

#include "common/types.h"

namespace freezing {

template<typename Clock>
class Ticker {
public:
  explicit Ticker(time_point<Clock> now,
                  std::optional<nanoseconds> tick_interval) :
      m_last_known_now{std::chrono::time_point_cast<nanoseconds>(now)},
      m_tick_interval{tick_interval} {}

  bool tick(time_point<Clock> now) {
    auto elapsed_time = calc_elapsed_time(now);
    m_last_known_now = now;

    if (!elapsed_time || !m_tick_interval) {
      m_time_since_last_tick = nanoseconds(0);
      return true;
    }
    m_time_since_last_tick += *elapsed_time;
    if (m_time_since_last_tick >= *m_tick_interval) {
      m_time_since_last_tick %= *m_tick_interval;
      return true;
    }
    return false;
  }

  void set_tick_interval(nanoseconds tick_interval) {
    m_tick_interval = tick_interval;
  }

  void clear_tick_interval() {
    m_tick_interval.reset();
  }

private:
  std::optional<time_point<Clock>> m_last_known_now;
  std::optional<nanoseconds> m_tick_interval;
  nanoseconds m_time_since_last_tick{};

  std::optional<nanoseconds> calc_elapsed_time(time_point<Clock> now) const {
    if (m_last_known_now) {
      return {now - *m_last_known_now};
    } else {
      return {};
    }
  }
};

}

#endif //NEPTUN_COMMON_TICKER_H
