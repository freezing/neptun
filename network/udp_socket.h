//
// Created by freezing on 12/03/2022.
//

#ifndef NEPTUN__UDP_SOCKET_H
#define NEPTUN__UDP_SOCKET_H

#include <vector>
#include <span>

#include "network.h"
#include "io_buffer.h"

namespace freezing::network {

template<typename Network>
class UdpSocket {
public:
  static UdpSocket bind(IpAddress ip, Network &network) {
    auto fd = network.udp_socket_ipv4();
    UdpSocket socket{network, fd, ip};
    socket.bind();
    return socket;
  }

  [[nodiscard]] std::span<std::uint8_t> read(std::span<std::uint8_t> buffer) {
    return m_network.read_from_socket(m_fd, buffer);
  }

  [[nodiscard]] std::size_t send_to(IpAddress ip_address,
                                    std::span<const std::uint8_t> payload) const {
    return m_network.send_to(m_fd, ip_address, payload);
  }

private:
  UdpSocket(Network &network, FileDescriptor fd, IpAddress ip)
      : m_network{network}, m_fd{fd}, m_ip{ip} {}

  void bind() {
    m_network.bind(m_fd, m_ip);
  }

  Network &m_network;
  FileDescriptor m_fd;
  IpAddress m_ip;

};

}

#endif //NEPTUN__UDP_SOCKET_H
