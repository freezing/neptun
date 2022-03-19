#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

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

class Ticker {
public:
  explicit Ticker(chrono::time_point<chrono::system_clock, chrono::nanoseconds> now,
                  chrono::nanoseconds tick_interval) :
      m_last_known_now{chrono::time_point_cast<chrono::nanoseconds>(now)},
      m_tick_interval{tick_interval},
      m_time_since_last_tick{tick_interval} {}

  bool tick(chrono::time_point<chrono::system_clock, chrono::nanoseconds> now) {
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
  chrono::time_point<chrono::system_clock, chrono::nanoseconds> m_last_known_now;
  chrono::nanoseconds m_tick_interval;
  chrono::nanoseconds m_time_since_last_tick;

};

int main(int argc, char **argv) {
  if (argc != 5) {
    cout << "Usage: <ip> <port> <peer_ip> <peer_port>" << endl;
    return -1;
  }

  auto ip = IpAddress::from_ipv4(argv[1], stoi(argv[2]));
  auto peer_ip = IpAddress::from_ipv4(argv[3], stoi(argv[4]));
  Neptun<OsNetwork> neptun{OS_NETWORK, ip};

  Ticker reliable_ticker(chrono::system_clock::now(), chrono::milliseconds(100));
  Ticker unreliable_ticker(chrono::system_clock::now(), chrono::milliseconds(300));

  std::vector<std::uint8_t> buffer(1600);
  u32 reliable_msg_seq_num = 0;
  u32 unreliable_msg_seq_num = 0;
  while (true) {
    auto now = chrono::system_clock::now();

    if (reliable_ticker.tick(now)) {
      neptun.send_reliable_to(peer_ip, [&reliable_msg_seq_num](byte_span buffer) {
        IoBuffer io{buffer};
        // TODO: Need io.serialized_size(s).
        std::string s = "Reliable " + to_string(reliable_msg_seq_num);

        usize serialized_size = sizeof(u16) + s.size();
        if (serialized_size > buffer.size()) {
          // Flow control kicking in.
          std::cout << "Reliable Flow Control kicking in." << std::endl;
          return byte_span{};
        }
        auto count = io.write_string(s, 0);
        reliable_msg_seq_num++;
        return buffer.first(count);
      });
    }

    if (unreliable_ticker.tick(now)) {
      neptun.send_unreliable_to(peer_ip, [&unreliable_msg_seq_num](byte_span buffer) {
        IoBuffer io{buffer};
        std::string s = "Unreliable " + to_string(unreliable_msg_seq_num);

        usize serialized_size = sizeof(u16) + s.size();
        if (serialized_size > buffer.size()) {
          // Flow control kicking in.
          std::cout << "Unreliable buffers are full." << std::endl;
          return byte_span{};
        }
        auto count = io.write_string(s, 0);
        unreliable_msg_seq_num++;
        return buffer.first(count);
      });
    }

    auto print_string = [](byte_span buffer) {
      IoBuffer io{buffer};
      auto s = io.read_string(0);
      std::cout << s << std::endl;
    };

    neptun.tick(chrono::time_point_cast<chrono::seconds>(now).time_since_epoch().count(),
                print_string,
                print_string);

    // Run this loop ~100 times a second.
    this_thread::sleep_for(chrono::milliseconds(1000));
  }
}
