//
// Created by freezing on 12/03/2022.
//

#ifndef NEPTUN__NETWORK_H_
#define NEPTUN__NETWORK_H_

#include <string>
#include <memory.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "ip_address.h"

namespace freezing::network {

struct FileDescriptor {
  int value;
};

class LinuxNetwork {
public:
  void bind(FileDescriptor file_descriptor, IpAddress ip_address) {
    // https://man7.org/linux/man-pages/man2/bind.2.html.
    if (::bind(file_descriptor.value, ip_address.as_sockaddr(), sizeof(sockaddr_in)) == -1) {
      throw std::runtime_error("Failed to bind the socket");
    }
  }

  FileDescriptor udp_socket_ipv4() {
    // https://man7.org/linux/man-pages/man2/socket.2.html
    int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == -1) {
      throw std::runtime_error("Couldn't create udp socket");
    }
    return FileDescriptor{fd};
  }
};

static const LinuxNetwork LINUX_NETWORK{};

}

#endif //NEPTUN__NETWORK_H_
