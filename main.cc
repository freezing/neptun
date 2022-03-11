#include <iostream>
#include <cassert>
#include <vector>
#include <span>
#include <string>
#include <thread>

#include "ring_buffer.h"
#include "udp.h"

using namespace std;

int _ring_buffer_main() {
  std::uint8_t data[] = {10, 20, 30, 40, 50, 60};
  RingBuffer<std::uint8_t> buffer{64};
  buffer.write(data);
  std::string sep;
  for (const std::uint8_t t : buffer) {
    printf("%s%d", sep.c_str(), t);
    sep = " ";
  }
  std::cout << std::endl;
  return 0;
}

[[noreturn]] int udp_main_server(int port) {
  cout << "UDP Server" << endl;
  std::vector<std::uint8_t> buffer;
  buffer.resize(1024);

  auto udp = UdpSocket::bind(port);
  while (true) {
    cout << "Reading from UDP socket." << endl;
    size_t num_bytes = udp.read(buffer.data(), buffer.size());
    buffer[num_bytes] = 0;
    std::string data((char * )buffer.data());

    cout << "Read: " << num_bytes << " bytes. " << endl;
    cout << data << endl;
    this_thread::sleep_for(chrono::seconds(1));
  }
}

[[noreturn]] int udp_main_client(int port, int server_port, const std::string& data) {
  cout << "UDP Client" << endl;

  struct sockaddr_in destination{};
  destination.sin_port = htons(server_port);
  inet_pton(AF_INET, "0.0.0.0", &(destination.sin_addr)); // IPv4

  auto udp = UdpSocket::bind(port);
  while (true) {
    size_t num_bytes = udp.send((void *) data.c_str(), data.length(), destination);
    cout << "Sent: " << num_bytes << endl;
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