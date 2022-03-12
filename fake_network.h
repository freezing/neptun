//
// Created by freezing on 12/03/2022.
//

#ifndef NEPTUN__FAKE_NETWORK_H
#define NEPTUN__FAKE_NETWORK_H

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "network.h"
#include "ip_address.h"

namespace freezing::network::testing {

namespace detail {

struct IpAddressOrSocketPredicate {
  FileDescriptor socket_fd;
  IpAddress ip;

  bool operator()(std::pair<IpAddress, FileDescriptor> candidate) const {
    return ip.to_string() == candidate.first.to_string()
        || socket_fd.value == candidate.second.value;
  };
};

}

class FakeNetwork {
public:
  void bind(FileDescriptor socket_fd, IpAddress ip_address) {

    auto it = std::find_if(
        std::begin(m_bind), std::end(m_bind), detail::IpAddressOrSocketPredicate{socket_fd, ip_address});

    if (it != m_bind.end()) {
      throw std::runtime_error("IP " + ip_address.to_string() + " is already bind to a socket "
                                   + std::to_string(socket_fd.value));
    }
  }

  FileDescriptor udp_socket_ipv4() {
    int fd = m_next_fd++;
    return FileDescriptor{fd};
  }

private:
  int m_next_fd{};
  std::vector<std::pair<IpAddress, FileDescriptor>> m_bind{};
};

}

#endif //NEPTUN__FAKE_NETWORK_H