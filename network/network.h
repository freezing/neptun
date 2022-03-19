#ifndef NEPTUN__NETWORK_H_
#define NEPTUN__NETWORK_H_

#define PLATFORM_WINDOWS 1
#define PLATFORM_MAC 2
#define PLATFORM_UNIX 3

#if defined(_WIN32)
#define PLATFORM PLATFORM_WINDOWS
#elif defined (__APPLE__)
#define PLATFORM PLATFORM_MAC
#else
#define PLATFORM PLATFORM_UNIX
#endif

#if PLATFORM == PLATFORM_WINDOWS

#include <winsock2.h>
#pragma comment( lib, "wsock32.lib" )

#elif PLATFORM == PLATFORM_MAC || PLATFORM == PLATFORM_UNIX

#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/socket.h>

#endif

#include <span>
#include <compare>
#include <string>
#include <memory>
#include <memory.h>

#include "common/types.h"
#include "network/ip_address.h"

namespace freezing::network {

namespace detail {

const int kNoFlags = 0;

}

struct FileDescriptor {
  int value;

  auto operator<=>(const FileDescriptor &other) const = default;
};

struct ReadPacketInfo {
  IpAddress sender;
  byte_span payload;
};

class OsNetwork {
public:
  void bind(FileDescriptor file_descriptor, IpAddress ip_address) {
    // https://man7.org/linux/man-pages/man2/bind.2.html.
    if (::bind(file_descriptor.value, ip_address.as_sockaddr(), sizeof(sockaddr_in)) == -1) {
      throw std::runtime_error("Failed to bind the socket");
    }
  }

  void initialize() {
#if PLATFORM == PLATFORM_WINDOWS
    // We must call WSAStartup to initialize the sockets layer before calling any socket functions
    // on Windows.
    WSADATA wsa_data;
    auto status = WSAStartup( MAKEWORD(2, 2), &wsa_data);
    if (status != NO_ERROR) {
      throw std::runtime_error("Failed to initialize socket on Windows.");
    }
#endif
  }

  void shutdown() {
#if PLATFORM == PLATFORM_WINDOWS
    // We must cleanup the windows sockets library when we are done with it.
    WSACleanup();
#endif
  }

  FileDescriptor udp_socket_ipv4() {
    // https://man7.org/linux/man-pages/man2/socket.2.html
    int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == -1) {
      throw std::runtime_error("Couldn't create udp socket");
    }
    return FileDescriptor{fd};
  }

  void set_non_blocking(FileDescriptor fd) {
#if PLATFORM == PLATFORM_WINDOWS
    // Windows does not provide the “fcntl” function, so we use the “ioctlsocket” function instead.
    DWORD non_blocking = 1;
    if (ioctlsocket(fd.value, FIONBIO, &non_blocking) != 0) {
      throw std::runtime_error("Failed to set socket as non-blocking: " + std::to_string(fd.value));
    }
#elif PLATFORM == PLATFORM_MAC || PLATFORM == PLATFORM_UNIX
    int non_blocking = 1;
    if (fcntl(fd.value, F_SETFL, O_NONBLOCK, non_blocking) == -1) {
      throw std::runtime_error("Failed to set socket as non-blocking: " + std::to_string(fd.value));
    }
#endif
  }

  void close_socket(FileDescriptor fd) {
#if PLATFORM == PLATFORM_WINDOWS
    if (closesocket(fd.value) == SOCKET_ERROR) {
      int last_error = WSAGetLastError();
      throw std::runtime_error("Failed to close socket: " + std::to_string(fd.value)
        + " with error: " + std::to_string(last_error));
    }
#elif PLATFORM == PLATFORM_MAC || PLATFORM == PLATFORM_UNIX
    if (close(fd.value) == -1) {
      throw std::runtime_error("Failed to close socket: " + std::to_string(fd.value));
    }
#endif
  }

  // TODO: Use expect to return errors.
  std::optional<ReadPacketInfo> read_from_socket(FileDescriptor fd,
                                                 std::span<std::uint8_t> buffer) {
    // https://linux.die.net/man/2/recvfrom
    sockaddr_in sender_ip{};
    socklen_t sender_ip_length = sizeof(struct sockaddr_in);

    ssize_t read_bytes = ::recvfrom(fd.value,
                                    static_cast<void *>(buffer.data()),
                                    buffer.size(),
                                    detail::kNoFlags,
                                    (struct sockaddr *) &sender_ip,
                                    &sender_ip_length);

    // TODO: Need to handle WOULDBLOCK error for windows.

    if (read_bytes == -1 && (errno == EWOULDBLOCK)) {
      return {};
    } else if (read_bytes == -1) {
      throw std::runtime_error("Failed to read data from the socket: " + std::to_string(fd.value));
    } else if (read_bytes == 0) {
      return {};
    } else {
      return {{IpAddress(sender_ip), buffer.subspan(0, read_bytes)}};
    }
  }

  std::size_t send_to(FileDescriptor fd,
                      IpAddress ip_address,
                      std::span<const std::uint8_t> payload) {
    // https://linux.die.net/man/2/sendto
    ssize_t sent_bytes = ::sendto(fd.value,
                                  static_cast<const void *>(payload.data()),
                                  payload.size(),
                                  detail::kNoFlags,
                                  ip_address.as_sockaddr(),
                                  sizeof(sockaddr_in));
    // When sending a UDP packet, we expect all data to be sent.
    if (sent_bytes != payload.size()) {
      std::string error = "unknown";
      auto error_code = errno;
      switch (error_code) {
      case EWOULDBLOCK:
        error = "WOULD_BLOCK";
        break;
      default:
        error = "unknown code: " + std::to_string(error_code);
      }
      throw std::runtime_error("Failed to sent bytes to: " + ip_address.to_string() + " " + error);
    }
    return static_cast<std::size_t>(sent_bytes);
  }
};

static OsNetwork OS_NETWORK{};

}

#endif //NEPTUN__NETWORK_H_
