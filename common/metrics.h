//
// Created by freezing on 20/03/2022.
//

#ifndef NEPTUN_COMMON_METRICS_H
#define NEPTUN_COMMON_METRICS_H

namespace freezing {


template<typename Key>
static constexpr usize metric_key_count();

template<typename Key>
static constexpr usize metric_key_index(Key);

template<typename Key>
static std::string metric_key_unit(Key);

template<typename Key, typename Value>
static double metric_key_unit_scale(Key key, Value value);

template<typename Key>
static std::string metric_key_name(Key);

// TODO: Should be [function_view]
template<typename Key>
static void foreach_key(const std::function<void(Key)>&);

template<typename Key, typename Value>
class Metrics {
public:
  explicit Metrics(std::string&& name) : m_name{std::move(name)} {}

  void inc(Key key, Value value) {
    m_values[metric_key_index(key)] += value;
  }

  void inc(Key key) {
    inc(key, 1);
  }

  Value value(Key key) const {
    return m_values[metric_key_index(key)];
  }

  Value value_by_index(usize index) const {
    return m_values[index];
  }

  const std::string& name() const {
    return m_name;
  }

private:
  std::array<Value, metric_key_count<Key>()> m_values{};
  std::string m_name;
};

template<typename Key, typename Value>
std::ostream& operator<<(std::ostream& os, const Metrics<Key, Value>& metrics) {
  os << metrics.name() << std::endl;
  foreach_key<Key>([&os, &metrics](Key key) {
    os << "    " << metric_key_name(key) << ": " << metric_key_unit_scale<Key, Value>(key, metrics.value(key)) << " " << metric_key_unit<Key>(key) << std::endl;
  });
  return os;
}

}

#endif //NEPTUN_COMMON_METRICS_H
