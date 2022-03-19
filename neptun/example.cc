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

constexpr u64 kReliableMessageIntervalMs = 1000;

int main(int argc, char **argv) {
  if (argc != 5) {
    cout << "Usage: <ip> <port> <peer_ip> <peer_port>" << endl;
    return -1;
  }

  auto ip = IpAddress::from_ipv4(argv[1], stoi(argv[2]));
  auto peer_ip = IpAddress::from_ipv4(argv[3], stoi(argv[4]));
  Neptun<OsNetwork> neptun{OS_NETWORK, ip};

  std::vector<std::uint8_t> buffer(1600);
  u64 time_since_last_reliable_msg_ms = 0;
  auto previous_tick_time = chrono::system_clock::now();
  u32 reliable_msg_seq_num = 0;
  while (true) {
    auto now = chrono::system_clock::now();
    auto elapsed_time = chrono::duration_cast<chrono::milliseconds>(now - previous_tick_time).count();

    // Send one reliable message every second.
    time_since_last_reliable_msg_ms += elapsed_time;
    // TODO: Create a concept that ticks given frequency.
    if (time_since_last_reliable_msg_ms >= kReliableMessageIntervalMs) {
      std::cout << "Sending reliable message" << std::endl;
      time_since_last_reliable_msg_ms -= kReliableMessageIntervalMs;
      neptun.send_reliable_to(peer_ip, [&reliable_msg_seq_num](byte_span buffer) {
        reliable_msg_seq_num++;
        IoBuffer io{buffer};
        auto count = io.write_string("Hey there, I'm a [Reliable Message " + to_string(reliable_msg_seq_num) + "]", 0);
        return buffer.first(count);
      });
    }

    neptun.tick(elapsed_time, [](byte_span buffer) {
      std::cout << "Read: " << span_to_string(buffer) << std::endl;
    });

    previous_tick_time = now;

    // Run this loop ~100 times a second.
    this_thread::sleep_for(chrono::milliseconds(10));
  }
}
