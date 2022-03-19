//
// Created by freezing on 13/03/2022.
//

#ifndef NEPTUN_NEPTUN_NEPTUN_H
#define NEPTUN_NEPTUN_NEPTUN_H

#include <map>
#include <stack>
#include <queue>

#include "common/types.h"
#include "network/network.h"
#include "network/udp_socket.h"
#include "neptun/messages/packet_header.h"
#include "neptun/packet_delivery_manager.h"
#include "neptun/reliable_stream.h"
#include "neptun/unreliable_stream.h"

namespace freezing::network {

namespace {
// Just above is used for reading.
constexpr usize kJustAboveMtu = 1600;
// JKust below is used for writing.
constexpr usize kJustBelowMtu = 1400;

struct Peer {
  PacketDeliveryManager packet_delivery_manager;
  ReliableStream reliable_stream;
  UnreliableStream unreliable_stream;
};

}

template<typename Network>
class Neptun {
public:
  explicit Neptun(Network &network,
                  IpAddress ip,
                  u32 packet_timeout = detail::kDefaultPacketTimeSeconds) : m_udp_socket{
      UdpSocket<Network>::bind(ip, network)}, m_network_buffer(kJustAboveMtu), m_packet_timeout{
      packet_timeout} {}

  template<typename OnReliableFn = std::function<void(byte_span)>, typename OnUnreliableFn = std::function<
      void(byte_span)>>
  void tick(u64 now,
            OnReliableFn on_reliable = [](byte_span) {},
            OnUnreliableFn on_unreliable = [](byte_span) {}) {
    for (auto&[ip, peer] : m_peers) {
      auto delivery_statuses = peer.packet_delivery_manager.drop_old_packets(now);
      process_delivery_statuses(peer, delivery_statuses);
    }
    read(now, on_reliable, on_unreliable);
    write(now);
  }

  template<typename WriteToBufferFn>
  void send_reliable_to(IpAddress ip, WriteToBufferFn write_to_buffer) {
    auto
        &reliable_stream = find_or_create_peer(0 /* next_expected_packet_id */, ip).reliable_stream;
    reliable_stream.template send(write_to_buffer);
  }

  template<typename WriteToBufferFn>
  void send_unreliable_to(IpAddress ip, WriteToBufferFn write_to_buffer) {
    auto &unreliable_stream =
        find_or_create_peer(0 /* next_expected_packet_id */, ip).unreliable_stream;
    unreliable_stream.template send(write_to_buffer);
  }

private:
  // These can be organized into a single network handler (but i need a good name).
  // e.g. std::map<IpAddress, SingleClientHandler> handlers, where SingleClientHandler has
  // DeliveryStatusNotification, ReliableStream, etc.
  std::map<IpAddress, Peer> m_peers;
  UdpSocket<Network> m_udp_socket;
  std::vector<u8> m_network_buffer{};
  u32 m_packet_timeout;

  Peer &find_or_create_peer(u32 next_expected_packet_id, IpAddress peer_ip) {
    if (!m_peers.contains(peer_ip)) {
      PacketDeliveryManager packet_delivery_manager{next_expected_packet_id, m_packet_timeout};
      ReliableStream reliable_stream{};
      UnreliableStream unreliable_stream{};
      m_peers.insert({peer_ip,
                      Peer{std::move(packet_delivery_manager), std::move(reliable_stream),
                           std::move(unreliable_stream)}});
    }
    return m_peers.find(peer_ip)->second;
  }

  template<typename OnReliableFn, typename OnUnreliableFn>
  void read(u64 now,
            OnReliableFn on_reliable,
            OnUnreliableFn on_unreliable = [](byte_span payload) {}) {
    byte_span network_buffer(m_network_buffer.begin(), m_network_buffer.begin() + kJustAboveMtu);
    std::optional<ReadPacketInfo> packet_info = m_udp_socket.read(network_buffer);
    if (!packet_info) {
      // There are no packets in the stream.
      return;
    }
    auto buffer = packet_info->payload;
    auto &peer = find_or_create_peer(0 /* next_expected_packet_id */, packet_info->sender);

    // Packet Delivery Manager stage.
    auto[read_count, delivery_statuses, packet_id] = peer.packet_delivery_manager.process_read(
        buffer);
    if (read_count == 0) {
      return;
    }

    buffer = advance(buffer, read_count);
    process_delivery_statuses(peer, delivery_statuses);

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

  void write(u64 now) {
    for (auto&[ip, peer] : m_peers) {
      write_to_peer(now, ip, peer);
    }
  }

  void write_to_peer(u64 now, IpAddress ip, Peer &peer) {
    byte_span buffer(m_network_buffer.begin(), m_network_buffer.begin() + kJustBelowMtu);

    // Packet Delivery Manager stage.
    auto packet_header_count = peer.packet_delivery_manager.write(buffer, now);
    // TODO: Write should return packet header (or at least id).
    auto packet_header = PacketHeader(buffer.first(packet_header_count));
    buffer = advance(buffer, packet_header_count);

    // Reliable Stream stage.
    auto reliable_stream_count = peer.reliable_stream.write(packet_header.id(), buffer);
    buffer = advance(buffer, reliable_stream_count);

    // Unreliable Stream stage.
    auto unreliable_stream_count = peer.unreliable_stream.write(buffer);
    buffer = advance(buffer, unreliable_stream_count);

    // Send to the peer. For many peers, we can buffer all packets and send them in one go with
    // "send to many" syscall (at least on Linux).
    byte_span payload(m_network_buffer.data(), packet_header_count + reliable_stream_count + unreliable_stream_count);
    auto sent_count = m_udp_socket.send_to(ip, payload);
    assert(sent_count == payload.size());
  }

  static void process_delivery_statuses(Peer &peer, DeliveryStatuses delivery_statuses) {
    delivery_statuses.template for_each([&peer](u32 packet_id, PacketDeliveryStatus status) {
      peer.reliable_stream.on_packet_delivery_status(packet_id, status);
    });
  }
};

}

#endif //NEPTUN_NEPTUN_NEPTUN_H
