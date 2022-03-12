//
// Created by freezing on 12/03/2022.
//

#ifndef NEPTUN__UDP_SOCKET_H
#define NEPTUN__UDP_SOCKET_H

#include "network.h"

namespace freezing::network {

template<typename Network = LinuxNetwork>
class UdpSocket {
public:
  static UdpSocket create(IpAddress ip, Network& network) {
    auto fd = network.udp_socket();
    return {network, fd, ip};
  }

  UdpSocket bind() {
    m_network.bind(m_fd, m_ip);
  }

private:
  UdpSocket(Network& network, FileDescriptor fd, IpAddress ip) : m_network{network}, m_fd{fd}, m_ip{ip} {}

  Network& m_network;
  FileDescriptor m_fd;
  IpAddress m_ip;
};

}

#endif //NEPTUN__UDP_SOCKET_H
