#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

#include "network/udp_socket.h"
#include "network/message.h"
#include "neptun.h"
#include "neptun/messages/chat_text.h"
#include "neptun/messages/segment.h"

using namespace std;
using namespace freezing::network;

int main(int argc, char **argv) {
  if (argc != 5) {
    cout << "Usage: <ip> <port> <peer_ip> <peer_port>" << endl;
    return -1;
  }

  auto ip = IpAddress::from_ipv4(argv[1], stoi(argv[2]));
  auto peer_ip = IpAddress::from_ipv4(argv[3], stoi(argv[4]));
  auto udp = UdpSocket<LinuxNetwork>::bind(ip, LINUX_NETWORK);

  std::vector<std::uint8_t> buffer(1600);

  std::uint64_t packet_id = 0;
  while (true) {
    // Write packet.
    auto now = chrono::system_clock::now();
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
    auto payload = udp.read(buffer);
    auto read_ping = Ping(payload);
    cout << "Read: Ping(timestamp = " << read_ping.timestamp() << ", note = " << read_ping.note()
         << endl;

    this_thread::sleep_for(chrono::seconds(1));
  }
}
