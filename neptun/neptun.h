//
// Created by freezing on 13/03/2022.
//

#ifndef NEPTUN_NEPTUN_NEPTUN_H
#define NEPTUN_NEPTUN_NEPTUN_H

#include <map>
#include <stack>
#include <queue>
#include <cmath>

#include "common/types.h"
#include "common/ticker.h"
#include "network/network.h"
#include "network/udp_socket.h"
#include "neptun/messages/packet_header.h"
#include "neptun/packet_delivery_manager.h"
#include "neptun/reliable_stream.h"
#include "neptun/unreliable_stream.h"
#include "neptun/neptun_metrics.h"
#include "neptun/connection_manager.h"

namespace freezing::network {

namespace {
// Just above is used for reading.
constexpr usize kJustAboveMtu = 1600;
// JKust below is used for writing.
constexpr u16 kJustBelowMtu = 1400;

template<typename Clock>
struct Peer {
  Ticker<Clock> send_packet_ticker;
  PacketDeliveryManager<Clock> packet_delivery_manager;
  ConnectionManager connection_manager;
  ReliableStream reliable_stream;
  UnreliableStream unreliable_stream;

  void update_send_rate(u8 rate) {
    if (rate == 0) {
      send_packet_ticker.clear_tick_interval();
    } else {
      nanoseconds tick_interval = nanoseconds(seconds(1)) / rate;
      send_packet_ticker.set_tick_interval(tick_interval);
    }
  }
};

}

template<typename Network, typename Clock>
class Neptun {
public:
  explicit Neptun(Network &network,
                  IpAddress ip,
                  ConnectionManagerConfig connection_manager_config,
                  milliseconds packet_timeout = detail::kDefaultPacketTimeout) : m_udp_socket{
      UdpSocket<Network>::bind(ip, network)}, m_network_buffer(kJustAboveMtu),
                                                                            m_connection_manager_config{
                                                                                connection_manager_config},
                                                                            m_packet_timeout{
                                                                                packet_timeout} {}

  template<typename OnReliableFn = std::function<void(byte_span)>, typename OnUnreliableFn = std::function<
      void(byte_span)>>
  void tick(time_point<Clock> now,
            OnReliableFn on_reliable = [](byte_span) {},
            OnUnreliableFn on_unreliable = [](byte_span) {}) {
    for (auto&[ip, peer] : m_peers) {
      auto delivery_statuses = peer.packet_delivery_manager.drop_old_packets(now);
      process_delivery_statuses(peer, delivery_statuses);
    }
    read(now, on_reliable, on_unreliable);
    write(now);
  }

  void connect(IpAddress ip, time_point<Clock> now) {
    auto &connection_manager =
        find_or_create_peer(0 /* next_expected_packet_id */, ip, now).connection_manager;
    connection_manager.connect();
  }

  bool is_connected(IpAddress ip) const {
    auto it = m_peers.find(ip);
    return it != m_peers.end() && it->second.connection_manager.is_peer_connected();
  }

  template<typename WriteToBufferFn>
  // TODO(nikola): Remove now from here and other APIs. It's currently only used to initialize peer, but that is not required anymore.
  void send_reliable_to(IpAddress ip, WriteToBufferFn write_to_buffer, time_point<Clock> now) {
    assert(is_connected(ip));
    auto
        &reliable_stream =
        find_or_create_peer(0 /* next_expected_packet_id */, ip, now).reliable_stream;
    reliable_stream.template send(write_to_buffer);
  }

  template<typename WriteToBufferFn>
  void send_unreliable_to(IpAddress ip, WriteToBufferFn write_to_buffer, time_point<Clock> now) {
    assert(is_connected(ip));
    auto &unreliable_stream =
        find_or_create_peer(0 /* next_expected_packet_id */, ip, now).unreliable_stream;
    unreliable_stream.template send(write_to_buffer);
  }

  const NeptunMetrics &metrics() const {
    return m_metrics;
  }

private:
  // These can be organized into a single network handler (but i need a good name).
  // e.g. std::map<IpAddress, SingleClientHandler> handlers, where SingleClientHandler has
  // DeliveryStatusNotification, ReliableStream, etc.
  std::map<IpAddress, Peer<Clock>> m_peers;
  UdpSocket<Network> m_udp_socket;
  std::vector<u8> m_network_buffer{};
  milliseconds m_packet_timeout;
  ConnectionManagerConfig m_connection_manager_config;
  NeptunMetrics m_metrics{"Neptun metrics"};

  Peer<Clock> &find_or_create_peer(PacketId next_expected_packet_id,
                                   IpAddress peer_ip,
                                   time_point<Clock> now) {
    if (!m_peers.contains(peer_ip)) {
      Ticker send_packet_ticker{now, {}};
      PacketDeliveryManager<Clock>
          packet_delivery_manager{next_expected_packet_id, m_packet_timeout};
      ConnectionManager connection_manager{m_connection_manager_config};
      ReliableStream reliable_stream{};
      UnreliableStream unreliable_stream{};
      m_peers.insert({peer_ip,
                      Peer<Clock>{std::move(send_packet_ticker),
                                  std::move(packet_delivery_manager),
                                  std::move(connection_manager),
                                  std::move(reliable_stream),
                                  std::move(unreliable_stream)}});
    }
    return m_peers.find(peer_ip)->second;
  }

  template<typename OnReliableFn, typename OnUnreliableFn>
  void read(time_point<Clock> now,
            OnReliableFn on_reliable,
            OnUnreliableFn on_unreliable = [](byte_span payload) {}) {
    byte_span network_buffer(m_network_buffer.begin(), m_network_buffer.begin() + kJustAboveMtu);
    std::optional<ReadPacketInfo> packet_info = m_udp_socket.read(network_buffer);
    if (!packet_info) {
      // There are no packets in the stream.
      return;
    }
    auto buffer = packet_info->payload;
    auto &peer = find_or_create_peer(0 /* next_expected_packet_id */, packet_info->sender, now);

    // Packet Delivery Manager stage.
    auto[read_count, delivery_statuses, packet_id] = peer.packet_delivery_manager.process_read(
        buffer);
    if (read_count == 0) {
      return;
    }
    buffer = advance(buffer, read_count);
    process_delivery_statuses(peer, delivery_statuses);

    // Connection Manager Stage.
    auto connection_manager_result = peer.connection_manager.read(buffer);
    if (!connection_manager_result) {
      // TODO: Drop connection.
      std::cerr << "Malformed packet received from the peer: " << packet_info->sender.to_string()
                << std::endl;
      // Ignore the rest of the data.
      return;
    }
    buffer = advance(buffer, *connection_manager_result);

    if (!peer.connection_manager.is_peer_connected()) {
      // Don't process messages unless the connection has been established.
      return;
    }
    // TODO: I can set this only when it's changed. I think that would be more readable and make
    // it more obvious that this doesn't change every tick.
    // I should change the ConnectionManager::on_packet API to return the read info.
    // Furthermore, this would mean I don't need [is_handshake_successful] function.
    auto peer_max_read_packet_rate = peer.connection_manager.peer_limit()->max_read_packet_rate;
    auto self_max_send_packet_rate = m_connection_manager_config.limit.max_send_packet_rate;

    if (peer_max_read_packet_rate == 0) {
      peer.update_send_rate(self_max_send_packet_rate);
    } else if (self_max_send_packet_rate == 0) {
      peer.update_send_rate(peer_max_read_packet_rate);
    } else {
      peer.update_send_rate(std::min(peer_max_read_packet_rate, self_max_send_packet_rate));
    }

    // Reliable Stream stage.
    auto
        reliable_stream_result = peer.reliable_stream.template read(packet_id, buffer, on_reliable);
    if (!reliable_stream_result) {
      // Packet is malformed, ignore the rest of it and drop connection to the peer.
      // TODO: What does it mean to drop the connection? We can't prevent them from sending
      // us packets.
      // For now, we are just logging it.
      // TODO: Use proper logging.
      std::cerr << "Malformed packet received from the peer: " << packet_info->sender.to_string()
                << std::endl;
      // Ignore the rest of the data.
      return;
    }
    buffer = advance(buffer, *reliable_stream_result);

    // Unreliable Stream stage.
    auto unreliable_stream_result =
        peer.unreliable_stream.template read(buffer, on_unreliable);
    if (!unreliable_stream_result) {
      std::cerr << "Malformed packet received from the peer: " << packet_info->sender.to_string()
                << std::endl;
      // Ignore the rest of the data.
      return;
    }
    buffer = advance(buffer, *unreliable_stream_result);
  }

  void write(time_point<Clock> now) {
    for (auto&[ip, peer] : m_peers) {
      auto bandwidth_limit = peer.connection_manager.peer_limit();
      u16 max_send_packet_size = m_connection_manager_config.limit.max_send_packet_size;
      if (bandwidth_limit) {
        max_send_packet_size = std::min(bandwidth_limit->max_read_packet_size, max_send_packet_size);
      }
      // TODO: Cleanup this. I want to give priority to connection manager sending packets and not
      // having to wait.
      // I probably want to start rate limitting packets when both sides agree that the
      // connection has been established.
      // This means that the server has seen ACK for its response.
      // Otherwise, it makes no sense to send any other messages since the client wouldn't know
      // what's the acceptable limit.
      if (peer.send_packet_ticker.tick(now) || !peer.connection_manager.is_fully_connected()) {
        write_to_peer(now, ip, peer, max_send_packet_size);
      }
    }
  }

  void write_to_peer(time_point<Clock> now,
                     IpAddress ip,
                     Peer<Clock> &peer,
                     u16 max_send_packet_size) {
    byte_span buffer(m_network_buffer.begin(),
                     m_network_buffer.begin() + std::min(kJustBelowMtu, max_send_packet_size));

    // Packet Delivery Manager stage.
    auto packet_header_count = peer.packet_delivery_manager.write(buffer, now);
    // TODO: Write should return packet header (or at least id).
    auto packet_header = PacketHeader(buffer.first(packet_header_count));
    buffer = advance(buffer, packet_header_count);

    // Connection Manager Stage.
    auto connection_manager_count = peer.connection_manager.write(packet_header.id(), buffer);
    buffer = advance(buffer, connection_manager_count);

    // Reliable Stream stage.
    auto reliable_stream_count = peer.reliable_stream.write(packet_header.id(), buffer);
    buffer = advance(buffer, reliable_stream_count);

    // Unreliable Stream stage.
    auto unreliable_stream_count = peer.unreliable_stream.write(buffer);
    buffer = advance(buffer, unreliable_stream_count);

    // Send to the peer. For many peers, we can buffer all packets and send them in one go with
    // "send to many" syscall (at least on Linux).
    byte_span payload(m_network_buffer.data(),
        // TODO: I always forget to add count here. Make this less error prone.
                      packet_header_count + connection_manager_count + reliable_stream_count
                          + unreliable_stream_count);
    auto sent_count = m_udp_socket.send_to(ip, payload);
    assert(sent_count == payload.size());
  }

  void process_delivery_statuses(Peer<Clock> &peer, DeliveryStatuses delivery_statuses) {
    delivery_statuses.template for_each([this, &peer](PacketId packet_id,
                                                      PacketDeliveryStatus status) {
      peer.connection_manager.on_packet_status_delivery(packet_id, status);
      peer.reliable_stream.on_packet_delivery_status(packet_id, status);
      switch (status) {
        case PacketDeliveryStatus::ACK:
          m_metrics.inc(NeptunMetricKey::PACKET_ACKS);
          break;
        case PacketDeliveryStatus::DROP:
          m_metrics.inc(NeptunMetricKey::PACKET_DROPS);
          break;
        default:
          throw std::runtime_error("Unknown PacketDeliveryStatus");
      }
    });
  }
};

}

#endif //NEPTUN_NEPTUN_NEPTUN_H
