#ifndef NEPTUN__UDP_H_
#define NEPTUN__UDP_H_

#include <string>
#include <cstring>
#include <vector>
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>

struct Packet {
  std::vector<std::uint8_t> payload;
};

class UdpSocket {
 public:
  static UdpSocket bind(int port) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == -1) {
      throw std::runtime_error("Couldn't create udp socket");
    }
    std::cout << "Socket: " << fd << std::endl;

    struct sockaddr_in socket_address{};
    memset((char *) &socket_address, 0, sizeof(socket_address));

    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(port);
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind socket to port
    if (::bind(fd, (struct sockaddr *) &socket_address, sizeof(socket_address)) == -1) {
      throw std::runtime_error("Failed to bind the socket");
    }

    std::cout << "Bind to: " << std::string(inet_ntoa(socket_address.sin_addr))
              << " " << ntohs(socket_address.sin_port) << std::endl;

    return UdpSocket{fd, socket_address};
  }

  size_t read(void *buffer, size_t length) const {
    struct sockaddr sender_address{};
    // TODO: On linux it's int, on darwin is socklen_t
    socklen_t sender_address_length = sizeof(sender_address);
    auto rcv = ::recvfrom(m_socketfd, buffer, length, 0,
                          (struct sockaddr *) &sender_address, &sender_address_length);
    auto addr = ((struct sockaddr_in *) (&sender_address));
    std::cout << "recv: " << std::string(inet_ntoa(addr->sin_addr))
              << ":" << ntohs(addr->sin_port)
              << std::endl;
    return rcv;
  }

  size_t send(void *buffer, size_t length, struct sockaddr_in destination) const {
    // TODO: to_address should be specified somehow nicely.
    std::cout << "send: " << std::string(inet_ntoa(destination.sin_addr))
              << " " << ntohs(destination.sin_port) << std::endl;
    return ::sendto(m_socketfd, buffer, length, 0, (struct sockaddr *) &destination, sizeof(destination));
  }

 private:
  int m_socketfd;
  struct sockaddr_in m_socket_address;

  UdpSocket(int socketfd, struct sockaddr_in socket_address) : m_socketfd{socketfd}, m_socket_address{socket_address} {}
};

#endif //NEPTUN__NETWORK_H_