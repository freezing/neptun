//
// Created by freezing on 20/03/2022.
//

#ifndef NEPTUN_NEPTUN_NEPTUN_METRICS_H
#define NEPTUN_NEPTUN_NEPTUN_METRICS_H

#include <string>
#include <ostream>

#include "common/types.h"
#include "common/metrics.h"

namespace freezing::network {

enum NeptunMetricKey {
  PACKET_ACKS,
  PACKET_DROPS,
};

using NeptunMetrics = Metrics<NeptunMetricKey, u64>;

}

namespace freezing {

template<>
static constexpr usize metric_key_count<network::NeptunMetricKey>() {
  return 2;
}

template<>
static constexpr usize metric_key_index<network::NeptunMetricKey>(network::NeptunMetricKey key) {
  return key;
}

template<>
static std::string metric_key_name<network::NeptunMetricKey>(network::NeptunMetricKey key) {
  switch (key) {
  case network::PACKET_ACKS:
    return "packet_acks";
  case network::PACKET_DROPS:
    return "packet_drops";
  default:
    throw std::runtime_error("unknown key: " + std::to_string(key));
  }
}

template<>
static std::string metric_key_unit<network::NeptunMetricKey>(network::NeptunMetricKey key) {
  return "";
}

template<>
static double metric_key_unit_scale<network::NeptunMetricKey, double>(network::NeptunMetricKey key, double value) {
  return static_cast<double>(value);
}


template<>
static void foreach_key<network::NeptunMetricKey>(const std::function<void(network::NeptunMetricKey)>& fn) {
  for (usize i = 0; i < metric_key_count<network::NeptunMetricKey>(); i++) {
    auto key = static_cast<network::NeptunMetricKey>(i);
    fn(key);
  }
}

}

#endif //NEPTUN_NEPTUN_NEPTUN_METRICS_H
