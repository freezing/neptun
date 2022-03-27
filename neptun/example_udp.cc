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

using namespace std;
using namespace freezing;
using namespace freezing::network;

int main(int argc, char **argv) {
  if (argc != 5) {
    cout << "Usage: <ip> <port> <peer_ip> <peer_port>" << endl;
    return -1;
  }

  auto ip = IpAddress::from_ipv4(argv[1], stoi(argv[2]));
  auto peer_ip = IpAddress::from_ipv4(argv[3], stoi(argv[4]));
  auto udp = UdpSocket<OsNetwork>::bind(ip, OS_NETWORK);

  std::vector<std::uint8_t> buffer(1600);

  std::uint64_t packet_id = 0;
  while (true) {
    // Write packet.
    auto now = std::chrono::time_point_cast<milliseconds>(system_clock::now());
    auto timestamp = now.time_since_epoch().count();
    printf("Timestamp: %ld\n", timestamp);
    std::string note = "Hey there, I'm a UDP packet #: " + to_string(packet_id);
    packet_id++;
    auto written_data = Ping::write(buffer, timestamp, note);
    auto sent_count = udp.send_to(peer_ip, written_data);
    auto sent_ping = Ping(written_data);
    cout << "Send: Ping(timestamp = " << sent_ping.timestamp() << ", note = " << sent_ping.note()
         << endl;

    // Read packet.
    auto packet_info = udp.read(buffer);
    if (packet_info) {
      auto read_ping = Ping(packet_info->payload);
      cout << "Read [" << packet_info->sender.to_string() << "]: Ping(timestamp = "
           << read_ping.timestamp() << ", note = " << read_ping.note()
           << endl;
    }

    this_thread::sleep_for(chrono::seconds(1));
  }
}
