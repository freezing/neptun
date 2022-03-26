//
// Created by freezing on 20/03/2022.
//

#ifndef NEPTUN_NETWORK_NETWORK_METRICS_H
#define NEPTUN_NETWORK_NETWORK_METRICS_H

#include <string>
#include <ostream>

#include "common/types.h"
#include "common/metrics.h"

namespace freezing::network {

enum NetworkMetricKey {
  PACKETS_SENT,
  PACKETS_READ,
  PAYLOAD_INGRESS,
  PAYLOAD_EGRESS,
};

using NetworkMetrics = Metrics<NetworkMetricKey, u64>;

}

namespace freezing {

// TODO: Warning: explicit specialization cannot have a storage class.
template<>
static constexpr usize metric_key_count<network::NetworkMetricKey>() {
  return 4;
}

template<>
static constexpr usize metric_key_index<network::NetworkMetricKey>(network::NetworkMetricKey key) {
  return key;
}

template<>
static std::string metric_key_name<network::NetworkMetricKey>(network::NetworkMetricKey key) {
  switch (key) {
  case network::PACKETS_SENT:return "packets_sent";
  case network::PACKETS_READ:return "packets_read";
  case network::PAYLOAD_INGRESS:return "payload_ingress";
  case network::PAYLOAD_EGRESS:return "payload_egress";
  default:throw std::runtime_error("unknown key: " + std::to_string(key));
  }
}

template<>
static std::string metric_key_unit<network::NetworkMetricKey>(network::NetworkMetricKey key) {
  switch (key) {
  case network::PAYLOAD_INGRESS:
  case network::PAYLOAD_EGRESS:return "MB";
  default:return "";
  }
}

template<>
static double metric_key_unit_scale<network::NetworkMetricKey, double>(network::NetworkMetricKey key, double value) {
  switch (key) {
  case network::PAYLOAD_INGRESS:
  case network::PAYLOAD_EGRESS:return static_cast<double>(value) / 1024.0 / 1024.0;
  default:return static_cast<double>(value);
  }
}

template<>
static void foreach_key<network::NetworkMetricKey>(const std::function<void(network::NetworkMetricKey)> &fn) {
  for (usize i = 0; i < metric_key_count<network::NetworkMetricKey>(); i++) {
    auto key = static_cast<network::NetworkMetricKey>(i);
    fn(key);
  }
}

}

#endif //NEPTUN_NETWORK_NETWORK_METRICS_H
