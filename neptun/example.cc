#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

#include "common/ticker.h"
#include "network/udp_socket.h"
#include "network/message.h"
#include "neptun.h"
#include "neptun/common.h"
#include "neptun/packet_delivery_manager.h"
#include "neptun/messages/segment.h"
#include "neptun/messages/reliable_message.h"
#include "neptun/neptun.h"

using namespace std;
using namespace freezing;
using namespace freezing::network;

static std::string span_to_string(std::span<std::uint8_t> data) {
  std::string s{};
  for (char c : data) {
    s += c;
  }
  return s;
}

int main(int argc, char **argv) {
  if (argc != 5) {
    cout << "Usage: <ip> <port> <peer_ip> <peer_port>" << endl;
    return -1;
  }

  constexpr usize kNumReliableMsgsPerBatch = 10;
  constexpr usize kNumUnreliableMsgsPerBatch = 10;

  auto ip = IpAddress::from_ipv4(argv[1], stoi(argv[2]));
  auto peer_ip = IpAddress::from_ipv4(argv[3], stoi(argv[4]));
  Neptun<OsNetwork> neptun{OS_NETWORK, ip};

  Ticker reliable_ticker(chrono::system_clock::now(), chrono::milliseconds(0));
  Ticker unreliable_ticker(chrono::system_clock::now(), chrono::milliseconds(30));
  Ticker print_metrics_ticker(chrono::system_clock::now(), chrono::seconds(5));

  std::vector<std::uint8_t> buffer(1600);
  u32 reliable_msg_seq_num = 0;
  u32 unreliable_msg_seq_num = 0;
  usize chars_read = 0;
  NeptunMetrics last_metrics{"Neptun Rate"};
  NetworkMetrics last_network_metrics{"Network Rate"};
  std::chrono::sys_time<std::chrono::nanoseconds> last_print_metrics_time{};
  while (true) {
    auto now = chrono::system_clock::now();

    if (reliable_ticker.tick(now)) {
      for (usize i = 0; i < kNumReliableMsgsPerBatch; i++) {
        neptun.send_reliable_to(peer_ip, [&reliable_msg_seq_num](byte_span buffer) {
          IoBuffer io{buffer};
          // TODO: Need io.serialized_size(s).
          std::string s = "Reliable " + to_string(reliable_msg_seq_num);

          usize serialized_size = sizeof(u16) + s.size();
          if (serialized_size > buffer.size()) {
            // Flow control kicking in.
//            std::cout << "Reliable Flow Control kicking in." << std::endl;
            return byte_span{};
          }
          auto count = io.write_string(s, 0);
          reliable_msg_seq_num++;
          return buffer.first(count);
        });
      }
    }

    if (unreliable_ticker.tick(now)) {
      for (usize i = 0; i < kNumUnreliableMsgsPerBatch; i++) {
        neptun.send_unreliable_to(peer_ip, [&unreliable_msg_seq_num](byte_span buffer) {
          IoBuffer io{buffer};
          std::string s = "Unreliable " + to_string(unreliable_msg_seq_num);

          usize serialized_size = sizeof(u16) + s.size();
          if (serialized_size > buffer.size()) {
            // Flow control kicking in.
//            std::cout << "Unreliable buffers are full." << std::endl;
            return byte_span{};
          }
          auto count = io.write_string(s, 0);
          unreliable_msg_seq_num++;
          return buffer.first(count);
        });
      }
    }

    if (print_metrics_ticker.tick(now)) {
      // Print total
//      std::cout << neptun.metrics() << std::endl;

      // Print rate neptun
      Metrics<NeptunMetricKey, double> neptun_rate{"Neptun Rate /s"};
      foreach_key<NeptunMetricKey>([&neptun_rate, &neptun, last_metrics, last_print_metrics_time, now](
          NeptunMetricKey key) {
        double delta = neptun.metrics().value(key) - last_metrics.value(key);
        double seconds =
            std::chrono::duration_cast<std::chrono::seconds>(now - last_print_metrics_time).count();
        neptun_rate.inc(key, delta / seconds);
      });
      // Print rate network
      Metrics<NetworkMetricKey, double> network_rate{"Network Rate /s"};
      foreach_key<NetworkMetricKey>([&network_rate, last_network_metrics, last_print_metrics_time, now](
          NetworkMetricKey key) {
        double delta = OS_NETWORK_METRICS.value(key) - last_network_metrics.value(key);
        double seconds =
            std::chrono::duration_cast<std::chrono::seconds>(now - last_print_metrics_time).count();
        network_rate.inc(key, delta / seconds);
      });
      std::cout << neptun_rate << std::endl << network_rate << std::endl;

      last_metrics = neptun.metrics();
      last_network_metrics = OS_NETWORK_METRICS;
      // TODO: Can Ticker provide previous tick time?
      last_print_metrics_time = now;
    }

    auto print_string = [&chars_read](byte_span buffer) {
      IoBuffer io{buffer};
      auto s = io.read_string(0);
//      std::cout << s << std::endl;
      chars_read += s.size();
    };

    neptun.tick(chrono::time_point_cast<chrono::seconds>(now).time_since_epoch().count(),
                print_string,
                print_string);

    // Run this loop ~100 times a second.
//    this_thread::sleep_for(chrono::milliseconds(10));
  }
}
