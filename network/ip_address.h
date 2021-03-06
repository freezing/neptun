//
// Created by freezing on 12/03/2022.
//

#ifndef NEPTUN__IP_ADDRESS_H
#define NEPTUN__IP_ADDRESS_H

#include <string>
#include <memory.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "common/types.h"

namespace freezing::network {

namespace detail {
static std::uint32_t ip_string_to_host(std::string s) {
  const std::string delimiter = ".";
  size_t pos = 0;
  std::uint32_t ip = 0;
  while ((pos = s.find(delimiter)) != std::string::npos) {
    std::string token = s.substr(0, pos);
    std::uint32_t byte = std::stoi(token);
    ip = (0xffffff00) & (ip << 8) | (0x000000ff & byte);
    s.erase(0, pos + delimiter.length());
  }
  std::uint32_t byte = std::stoi(s);
  ip = (0xffffff00) & (ip << 8) | (0x000000ff & byte);
  return ip;
}

static std::string ip_host_to_string(std::uint32_t ip) {
  std::string s = "";
  std::string sep = "";
  auto next_token = [&s, &sep, &ip]() {
    std::uint32_t byte = (ip & 0xff000000) >> 24;
    s += sep + std::to_string(byte);
    sep = ".";
    ip <<= 8;
  };
  for (int i = 0; i < 4; i++) {
    next_token();
  }
  return s;
}
}

// TODO: Should port be part of IpAddress? If so, should the name reflect that?
// E.g. at the IP level we don't know anything about ports, so this is a bit weird.
class IpAddress {
public:
  static IpAddress from_ipv4(const std::string& ip, std::uint16_t port) {
    std::uint32_t host_endian_ip = detail::ip_string_to_host(ip);
    sockaddr_in socket_address{};
    memset((char *) &socket_address, 0, sizeof(socket_address));
    // AF_INET is for IPv4 (AF_INET6 is for IPv6)
    socket_address.sin_family = AF_INET;
    // Convert port to network-endian short (16-bits).
    socket_address.sin_port = htons(port);
    // Convert IP to network-endian long (32-bits).
    socket_address.sin_addr.s_addr = htonl(detail::ip_string_to_host(ip));
    return IpAddress{socket_address};
  }

  static IpAddress from_u32(u32 ip, std::uint16_t port) {
    sockaddr_in socket_address{};
    memset((char *) &socket_address, 0, sizeof(socket_address));
    // AF_INET is for IPv4 (AF_INET6 is for IPv6)
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(port);
    socket_address.sin_addr.s_addr = htonl(ip);
    return IpAddress{socket_address};
  }

  IpAddress(const IpAddress& other) = default;

  explicit IpAddress(sockaddr_in socket_address)
      : m_socket_address{socket_address} {}

  [[nodiscard]] sockaddr* as_sockaddr() const {
    return (sockaddr*) &m_socket_address;
  }

  [[nodiscard]] std::string to_string() const {
    return detail::ip_host_to_string(ip_as_host_long()) + ":" + std::to_string(port());
  }

  [[nodiscard]] std::uint16_t port() const {
    return ntohs(m_socket_address.sin_port);
  }

  bool operator< (const IpAddress& other) const {
    return ip_as_host_long() < other.ip_as_host_long()
      || (ip_as_host_long() == other.ip_as_host_long() && port() < other.port());
  }

  bool operator== (const IpAddress& other) const {
    return !(*this < other) && !(other < *this);
  }

private:
  sockaddr_in m_socket_address;

  [[nodiscard]] std::uint32_t ip_as_host_long() const {
    return ntohl(m_socket_address.sin_addr.s_addr);
  }
};

}

#endif //NEPTUN__IP_ADDRESS_H
