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
}

template<typename Network>
class Neptun {
public:
  explicit Neptun(Network &network, IpAddress ip) : m_udp_socket{
      UdpSocket<Network>::bind(ip, network)}, m_read_packet_buffer(kBigEnoughForMtu) {}

  template<typename OnReliableFn>
  void read(u64 now, OnReliableFn on_reliable) {
    // TODO: Implement Neptun process where it reads a packet, sends it to the
    // delivery manager, reliablestream, and finally writes it to the peer.
    auto payload = m_udp_socket.read(m_read_packet_buffer);
    auto packet_header = PacketHeader(payload);

  }

  template<typename WriteToBufferFn>
  void send_reliable_to(IpAddress ip, WriteToBufferFn write_to_buffer) {
    auto &reliable_stream = m_reliable_streams[ip];
    reliable_stream.template send(write_to_buffer);
  }

private:
  // These can be organized into a single network handler (but i need a good name).
  // e.g. std::map<IpAddress, SingleClientHandler> handlers, where SingleClientHandler has
  // DeliveryStatusNotification, ReliableStream, etc.
  std::map<IpAddress, PacketDeliveryManager> m_packet_delivery_managers;
  std::map<IpAddress, ReliableStream> m_reliable_streams{};
  std::map<IpAddress, UdpSocket<Network>> m_udp_socket;
  std::vector<u8> m_read_packet_buffer{};
};

}

#endif //NEPTUN_NEPTUN_NEPTUN_H
