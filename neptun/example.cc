#include <iostream>
#include <chrono>
#include <thread>

#include "network/udp_socket.h"
#include "network/message.h"

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

  IoBuffer io_buffer{};
  std::uint64_t packet_id = 0;
  while (true) {
    // Write packet.
    auto now = chrono::system_clock::now();
    auto timestamp = now.time_since_epoch().count();
    printf("Timestamp: %lld\n", timestamp);
    std::string note = "Hey there, I'm a UDP packet #: " + to_string(packet_id);
    packet_id++;
    io_buffer.flip();
    Ping<>::write(io_buffer, timestamp, note);
    io_buffer.flip();
    auto sent_count = udp.send_to(peer_ip, io_buffer.data());

    auto sent_ping = Ping(io_buffer);
    cout << "Send: Ping(timestamp = " << sent_ping.timestamp() << ", note = " << sent_ping.note()
         << endl;

    // Read packet.
    io_buffer.flip();
    auto payload = udp.read(io_buffer);
    io_buffer.flip();
    auto read_ping = Ping(io_buffer);
    cout << "Read: Ping(timestamp = " << read_ping.timestamp() << ", note = " << read_ping.note()
         << endl;

    this_thread::sleep_for(chrono::seconds(1));
  }
}
