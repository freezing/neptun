#include <iostream>
#include <cassert>
#include <vector>
#include <span>
#include <string>
#include <thread>

#include "ring_buffer.h"
#include "udp_socket.h"

using namespace std;
using namespace freezing::network;

std::span<const std::uint8_t> string_to_span(const std::string& s) {
  return std::span(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

[[noreturn]] int udp_main_server(int port) {
  cout << "UDP Server" << endl;

  IpAddress ip = IpAddress::from_ipv4("0.0.0.0", port);

  auto udp = UdpSocket<LinuxNetwork>::bind(ip, LINUX_NETWORK);
  while (true) {
    auto payload = udp.read();
    std::string s((char * ) payload.data(), payload.size());

    cout << "Read: " << payload.size() << " bytes: " << s << endl;
    this_thread::sleep_for(chrono::seconds(1));
  }
}

[[noreturn]] int udp_main_client(int port, int server_port, const std::string& data) {
  cout << "UDP Client" << endl;

  auto ip = IpAddress::from_ipv4("0.0.0.0", port);
  auto server_ip = IpAddress::from_ipv4("0.0.0.0", server_port);
  auto udp = UdpSocket<LinuxNetwork>::bind(ip, LINUX_NETWORK);

  while (true) {
    std::span payload = string_to_span(data);
    auto count = udp.send_to(server_ip, payload);
    cout << "Sent: " << count << endl;
    this_thread::sleep_for(chrono::seconds(1));
  }
}

int main(int argc, char** argv) {
  // TODO: Need server address for client.
  int port = stoi(argv[2]);

  if (strcmp(argv[1], "server") == 0) {
    udp_main_server(port);
  } else if (strcmp(argv[1], "client") == 0) {
    int server_port = stoi(argv[3]);
    udp_main_client(port, server_port, string(argv[4]));
  }
  cout << "Unknown arg value: " << string(argv[1]) << endl;
  return -1;
}