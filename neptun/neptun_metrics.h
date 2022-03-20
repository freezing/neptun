//
// Created by freezing on 20/03/2022.
//

#ifndef NEPTUN_NEPTUN_NEPTUN_METRICS_H
#define NEPTUN_NEPTUN_NEPTUN_METRICS_H

#include <array>
#include <string>
#include <ostream>

#include "common/types.h"

namespace freezing::network {

template<typename Key>
constexpr usize metric_key_count();

template<typename Key>
constexpr usize metric_key_index(Key);

template<typename Key>
std::string metric_key_name(Key);

// TODO: Should be [function_view]
template<typename Key>
void foreach_key(const std::function<void(Key)>&);

template<typename Key>
class Metrics {
public:
  explicit Metrics(std::string&& name) : m_name{std::move(name)} {}

  void inc(Key key, u64 value) {
    m_values[metric_key_index(key)] += value;
  }

  void inc(Key key) {
    inc(key, 1);
  }

  u64 value(Key key) const {
    return m_values[metric_key_index(key)];
  }

  u64 value_by_index(usize index) const {
    return m_values[index];
  }

  const std::string& name() const {
    return m_name;
  }

private:
  std::array<u64, metric_key_count<Key>()> m_values{};
  std::string m_name;
};

template<typename Key>
std::ostream& operator<<(std::ostream& os, const Metrics<Key>& metrics) {
  os << metrics.name() << std::endl;
  foreach_key<Key>([&os, &metrics](Key key) {
    os << "    " << metric_key_name(key) << ": " << metrics.value(key) << std::endl;
  });
  return os;
}

enum NeptunMetricKey {
  PACKETS_SENT,
  PACKETS_READ,
  PACKET_ACKS,
  PACKET_DROPS,
};

template<>
constexpr usize metric_key_count<NeptunMetricKey>() {
  return 4;
}

template<>
constexpr usize metric_key_index<NeptunMetricKey>(NeptunMetricKey key) {
  return key;
}

template<>
std::string metric_key_name<NeptunMetricKey>(NeptunMetricKey key) {
  switch (key) {
  case PACKETS_SENT:
    return "packets_sent";
  case PACKETS_READ:
    return "packets_read";
  case PACKET_ACKS:
    return "packet_acks";
  case PACKET_DROPS:
    return "packet_drops";
  default:
    throw std::runtime_error("unknown key: " + std::to_string(key));
  }
}

template<>
void foreach_key<NeptunMetricKey>(const std::function<void(NeptunMetricKey)>& fn) {
  for (usize i = 0; i < metric_key_count<NeptunMetricKey>(); i++) {
    auto key = static_cast<NeptunMetricKey>(i);
    fn(key);
  }
}

using NeptunMetrics = Metrics<NeptunMetricKey>;

}

#endif //NEPTUN_NEPTUN_NEPTUN_METRICS_H
