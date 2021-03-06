//
// Created by freezing on 12/03/2022.
//

#ifndef NEPTUN__FAKE_NETWORK_H
#define NEPTUN__FAKE_NETWORK_H

#include <algorithm>
#include <stdexcept>
#include <queue>
#include <vector>

#include "network/network.h"
#include "network/ip_address.h"

namespace freezing::network::testing {

namespace detail {

constexpr int kReasonableMtu = 1400;

struct PendingPacket {
  IpAddress sender;
  std::vector<u8> payload;
};

struct UdpPackets {
  std::queue<PendingPacket> packets;
};

struct SocketPredicate {
  FileDescriptor socket_fd;

  bool operator()(std::pair<IpAddress, FileDescriptor> candidate) const {
    return socket_fd.value == candidate.second.value;
  };
};

struct IpAddressOrSocketPredicate {
  FileDescriptor socket_fd;
  IpAddress ip;

  bool operator()(std::pair<IpAddress, FileDescriptor> candidate) const {
    return ip.to_string() == candidate.first.to_string()
        || socket_fd.value == candidate.second.value;
  };
};

}

struct Stats {
  // Number of packets sent the IP.
  u64 num_sent_packets{0};
  // Number of packets read by the IP.
  u64 num_read_packets{0};
  u64 num_sent_bytes{0};
  u64 num_read_bytes{0};
};

// Currently treats all sockets as UDP sockets!
class FakeNetwork {
public:
  // TODO: Remove copy constructor.
  explicit FakeNetwork(int mtu = detail::kReasonableMtu) : m_mtu{mtu} {}

  void bind(FileDescriptor fd, IpAddress ip_address) {
    if (!is_socket_open(fd)) {
      throw std::runtime_error("Cannot bind unknown socket: " + std::to_string(fd.value));
    }

    auto it = std::find_if(
        std::begin(m_bind),
        std::end(m_bind),
        detail::IpAddressOrSocketPredicate{fd, ip_address});

    if (it != m_bind.end()) {
      throw std::runtime_error("IP " + ip_address.to_string() + " is already bound to a socket "
                                   + std::to_string(fd.value));
    }

    m_bind.emplace_back(ip_address, fd);
  }

  FileDescriptor udp_socket_ipv4() {
    int fd = m_next_fd++;
    return FileDescriptor{fd};
  }

  void set_non_blocking(FileDescriptor fd) {
    // Fake sockets are always non-blocking for now.
  }

  // TODO: Rename to [read_from].
  std::optional<ReadPacketInfo> read_from_socket(FileDescriptor fd,
                                                 std::span<std::uint8_t> buffer) {
    auto ip = find_ip(fd);
    if (!ip) {
      throw std::runtime_error(
          "Failed to read data from unbound socket: " + std::to_string(fd.value));
    }
    auto &udp_packets = m_buffers[*ip];
    if (udp_packets.packets.empty()) {
      return {};
    }
    auto packet = std::move(udp_packets.packets.front());
    udp_packets.packets.pop();
    auto& stats = m_stats[*ip];
    stats.num_read_packets++;
    stats.num_read_bytes += packet.payload.size();

    // Any data in the packet that can't fit in the buffer is dropped.
    int idx = 0;
    for (auto it = buffer.begin(); it != buffer.end(); it++) {
      if (idx >= packet.payload.size()) {
        // No more data to read.
        break;
      }
      *it = packet.payload[idx++];
    }

    return {{packet.sender, {buffer.subspan(0, idx)}}};
  }

  [[nodiscard]] std::size_t send_to(FileDescriptor sender_fd,
                                    IpAddress ip_address,
                                    const_byte_span payload) {
    if (!is_socket_bound(sender_fd)) {
      throw std::runtime_error(
          "Failed to send data via unbound socket: " + std::to_string(sender_fd.value));
    }
    auto sender_ip = *find_ip(sender_fd);
    auto& stats = m_stats[sender_ip];
    stats.num_sent_packets++;
    stats.num_sent_bytes += payload.size();

    if (m_should_drop_packets) {
      // Even though the packet is dropped, we pretend it was sent.
      // This is what UDP socket would do.
      return payload.size();
    }

    auto &buffer = m_buffers[ip_address];
    // Safety: sender exists because [is_socket_bound] is satisfied.
    auto sender = *find_ip(sender_fd);
    buffer.packets.push({sender, std::vector(payload.begin(), payload.end())});
    // Limit packet payload to the size of MTU.
    auto &pending_packet = buffer.packets.back();
    if (pending_packet.payload.size() > m_mtu) {
      pending_packet.payload.erase(pending_packet.payload.begin() + m_mtu,
                                   pending_packet.payload.end());
    }
    if (m_should_log_packets) {
      IpAddress sender_ip = *find_ip(sender_fd);
      std::string payload_string = "<no formatter>";
      if (m_packet_formatter) {
        payload_string = (*m_packet_formatter)(payload);
      }
      std::cout << "Send[" << sender_ip.to_string() << " -> " << ip_address.to_string() << "] "
                << payload_string << std::endl;
    }
    return payload.size();
  }

  void log_packets(bool should_log) {
    m_should_log_packets = should_log;
  }

  void set_packet_formatter(std::function<std::string(byte_span)> packet_formatter) {
    m_packet_formatter = packet_formatter;
  }

  void drop_packets(bool should_drop_packets) {
    m_should_drop_packets = should_drop_packets;
  }

  Stats stats(IpAddress ip) const {
    auto it = m_stats.find(ip);
    if (it == m_stats.end()) {
      return Stats{};
    }
    return it->second;
  }

  void clear_stats() {
    m_stats.clear();
  }

private:
  std::optional<std::function<std::string(byte_span)>> m_packet_formatter{};
  bool m_should_log_packets{false};
  int m_mtu;
  int m_next_fd{0};
  bool m_should_drop_packets{false};
  std::vector<std::pair<IpAddress, FileDescriptor>> m_bind{};
  std::map<IpAddress, detail::UdpPackets> m_buffers{};
  std::map<IpAddress, Stats> m_stats;

  [[nodiscard]] bool is_socket_open(FileDescriptor fd) const {
    return fd.value < m_next_fd;
  }

  [[nodiscard]] bool is_socket_bound(FileDescriptor fd) const {
    return find_ip(fd).has_value();
  }

  [[nodiscard]] std::optional<IpAddress> find_ip(FileDescriptor socket_fd) const {
    auto it = std::find_if(
        std::begin(m_bind),
        std::end(m_bind),
        detail::SocketPredicate{socket_fd});

    if (it != m_bind.end()) {
      return {it->first};
    }
    return {};
  }
};

}

#endif //NEPTUN__FAKE_NETWORK_H