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
#include "neptun/messages/packet.h"
#include "neptun/packet_delivery_manager.h"
#include "neptun/reliable_stream.h"

namespace freezing::network {

namespace {
constexpr usize kBigEnoughForMtu = 1600;

struct Peer {
  PacketDeliveryManager packet_delivery_manager;
  ReliableStream reliable_stream;
};

}

template<typename Network>
class Neptun {
public:
  explicit Neptun(Network &network, IpAddress ip) : m_udp_socket{
      UdpSocket<Network>::bind(ip, network)}, m_network_buffer(kBigEnoughForMtu) {}

  template<typename OnReliableFn>
  void read(u64 now, OnReliableFn on_reliable) {
    std::optional<ReadPacketInfo> packet_info = m_udp_socket.read(m_network_buffer);
    if (!packet_info) {
      // There are no packets in the stream.
      return;
    }
    auto buffer = packet_info->payload;
    auto &peer = find_or_create_peer(0 /* next_expected_packet_id */, packet_info->sender);

    // Packet Delivery Manager stage.
    auto [read_count, delivery_statuses, packet_id] = peer.packet_delivery_manager.process_read(buffer);
    if (read_count == 0) {
      return;
    }

    buffer = advance(buffer, read_count);
    process_delivery_statuses(peer, delivery_statuses);

    // Reliable Stream stage.
    read_count = peer.reliable_stream.template read(packet_id, buffer, on_reliable);
    buffer = advance(buffer, read_count);
  }

  void write(u64 now) {
    for (auto& [ip, peer] : m_peers) {
      write_to_peer(now, ip, peer);
    }
  }

  template<typename WriteToBufferFn>
  void send_reliable_to(IpAddress ip, WriteToBufferFn write_to_buffer) {
    auto &reliable_stream = find_or_create_peer(0 /* next_expected_packet_id */, ip).reliable_stream;
    reliable_stream.template send(write_to_buffer);
  }

private:
  // These can be organized into a single network handler (but i need a good name).
  // e.g. std::map<IpAddress, SingleClientHandler> handlers, where SingleClientHandler has
  // DeliveryStatusNotification, ReliableStream, etc.
  std::map<IpAddress, Peer> m_peers;
  UdpSocket<Network> m_udp_socket;
  std::vector<u8> m_network_buffer{};

  Peer &find_or_create_peer(u32 next_expected_packet_id, IpAddress peer_ip) {
    if (!m_peers.contains(peer_ip)) {
      PacketDeliveryManager packet_delivery_manager{next_expected_packet_id};
      ReliableStream reliable_stream{};
      m_peers.insert({peer_ip, Peer{std::move(packet_delivery_manager), std::move(reliable_stream)}});
    }
    return m_peers.find(peer_ip)->second;
  }

  void write_to_peer(u64 now, IpAddress ip, Peer& peer) {
    byte_span buffer(m_network_buffer);

    // Packet Delivery Manager stage.
    auto packet_header_count = peer.packet_delivery_manager.write(buffer, now);
    // TODO: Write should return packet header (or at least id).
    auto packet_header = PacketHeader(buffer.first(packet_header_count));
    buffer = advance(buffer, packet_header_count);

    // Reliable Stream stage.
    auto reliable_stream_count = peer.reliable_stream.write(packet_header.id(), buffer);
    buffer = advance(buffer, reliable_stream_count);

    // Send to the peer. For many peers, we can buffer all packets and send them in one go with
    // "send to many" syscall (at least on Linux).
    byte_span payload(m_network_buffer.data(), packet_header_count + reliable_stream_count);
    auto sent_count = m_udp_socket.send_to(ip, payload);
    assert(sent_count == payload.size());
  }

  static void process_delivery_statuses(Peer& peer, DeliveryStatuses delivery_statuses) {
    delivery_statuses.template for_each([&peer](u32 packet_id, PacketDeliveryStatus status) {
      peer.reliable_stream.on_packet_delivery_status(packet_id, status);
    });
  }
};

}

#endif //NEPTUN_NEPTUN_NEPTUN_H
