//
// Created by freezing on 12/03/2022.
//

#ifndef NEPTUN__NETWORK_H_
#define NEPTUN__NETWORK_H_

#include <span>
#include <compare>
#include <string>
#include <memory>
#include <memory.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "ip_address.h"

namespace freezing::network {

namespace detail {

const int kNoFlags = 0;

}

struct FileDescriptor {
  int value;

  auto operator<=>(const FileDescriptor &other) const = default;
};

class LinuxNetwork {
public:
  void bind(FileDescriptor file_descriptor, IpAddress ip_address) {
    // https://man7.org/linux/man-pages/man2/bind.2.html.
    if (::bind(file_descriptor.value, ip_address.as_sockaddr(), sizeof(sockaddr_in)) == -1) {
      throw std::runtime_error("Failed to bind the socket");
    }
  }

  [[nodiscard]] FileDescriptor udp_socket_ipv4() {
    // https://man7.org/linux/man-pages/man2/socket.2.html
    int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == -1) {
      throw std::runtime_error("Couldn't create udp socket");
    }
    return FileDescriptor{fd};
  }

  [[nodiscard]] void set_non_blocking(FileDescriptor fd) {
    int non_blocking = 1;
    if (fcntl(fd.value, F_SETFL, O_NONBLOCK, non_blocking) == -1) {
      throw std::runtime_error("Failed to set socket as non-blocking: " + std::to_string(fd.value));
    }
  }

  // TODO: Use expect to return errors.
  [[nodiscard]] std::span<std::uint8_t> read_from_socket(FileDescriptor fd,
                                                         std::span<std::uint8_t> buffer) {
    // https://linux.die.net/man/2/recvfrom
    sockaddr_in sender_ip{};
    socklen_t sender_ip_length{};

    ssize_t read_bytes = ::recvfrom(fd.value,
                                    static_cast<void *>(buffer.data()),
                                    buffer.size(),
                                    detail::kNoFlags,
                                    (struct sockaddr *) &sender_ip,
                                    &sender_ip_length);

    if (read_bytes == -1) {
      throw std::runtime_error("Failed to read data from the socket: " + std::to_string(fd.value));
    }
    return buffer.subspan(0, read_bytes);
  }

  [[nodiscard]] std::size_t send_to(FileDescriptor fd,
                                    IpAddress ip_address,
                                    std::span<const std::uint8_t> payload) {
    // https://linux.die.net/man/2/sendto
    ssize_t sent_bytes = ::sendto(fd.value,
                                  static_cast<const void *>(payload.data()),
                                  payload.size(),
                                  detail::kNoFlags,
                                  (struct sockaddr *) &ip_address,
                                  sizeof(ip_address));
    if (sent_bytes == -1) {
      throw std::runtime_error("Failed to sent bytes to: " + ip_address.to_string());
    }
    return static_cast<std::size_t>(sent_bytes);
  }
};

static LinuxNetwork LINUX_NETWORK{};

}

#endif //NEPTUN__NETWORK_H_
