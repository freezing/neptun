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
      UdpSocket<Network>::bind(ip, network)}, m_read_packet_buffer(kBigEnoughForMtu) {}

  template<typename OnReliableFn>
  void read(u64 now, OnReliableFn on_reliable) {
    // TODO: Implement Neptun process where it reads a packet, sends it to the
    // delivery manager, reliable stream, and finally writes it to the peer.
    std::optional<ReadPacketInfo> packet_info = m_udp_socket.read(m_read_packet_buffer);
    if (!packet_info) {
      // There are no packets in the stream.
      return;
    }
    auto buffer = packet_info->payload;
    auto &peer = find_or_create_peer(packet_info->sender);

    // Packet Delivery Manager stage.
    auto [read_count, delivery_statuses, packet_id] = peer.packet_delivery_manager.process_read(buffer);
    buffer = buffer.last(buffer.size() - read_count);
    process_delivery_statuses(peer);

    // Reliable Stream stage.
    read_count = peer.reliable_stream.template read(packet_id, buffer, [this](auto payload) {
      on_reliable_message(payload);
    });
    buffer = buffer.last(buffer.size() - read_count);

    // TODO: Other managers.
  }

  template<typename WriteToBufferFn>
  void send_reliable_to(IpAddress ip, WriteToBufferFn write_to_buffer) {
    auto &reliable_stream = m_peers[ip].reliable_stream;
    reliable_stream.template send(write_to_buffer);
  }

private:
  // These can be organized into a single network handler (but i need a good name).
  // e.g. std::map<IpAddress, SingleClientHandler> handlers, where SingleClientHandler has
  // DeliveryStatusNotification, ReliableStream, etc.
  std::map<IpAddress, Peer> m_peers;
  UdpSocket<Network> m_udp_socket;
  std::vector<u8> m_read_packet_buffer{};

  Peer &find_or_create_peer(u32 packet_id, IpAddress peer_ip) {
    if (!m_peers.contains(peer_ip)) {
      PacketDeliveryManager packet_delivery_manager{packet_id};
      ReliableStream reliable_stream{};
      m_peers[peer_ip] = {std::move(packet_delivery_manager), std::move(reliable_stream)};
    }
    return m_peers[peer_ip];
  }

  void on_reliable_message(byte_span payload) {
    // TODO:
    std::cout << "Got reliable message of size: " << payload.size() << std::endl;
  }

  static void process_delivery_statuses(Peer& peer, DeliveryStatuses delivery_statuses) {
    delivery_statuses.template for_each([&peer](u32 packet_id, PacketDeliveryStatus status) {
      peer.reliable_stream.on_packet_delivery_status(packet_id, status);
    });
  }
};

}

#endif //NEPTUN_NEPTUN_NEPTUN_H
