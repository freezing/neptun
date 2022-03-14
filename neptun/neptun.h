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
#include "neptun/reliable_stream.h"

namespace freezing::network {

template<typename Network>
class Neptun {
public:
  explicit Neptun(Network &network) : m_network{network} {}

  template<typename OnReliableFn>
  void process(u64 now, OnReliableFn on_reliable) {
    // TODO: Implement Neptun process where it reads a packet, sends it to the
    // delivery manager, reliablestream, and finally writes it to the peer.
  }

  template<typename WriteToBufferFn>
  void send_reliable_to(IpAddress ip, WriteToBufferFn write_to_buffer) {
    auto &reliable_stream = m_reliable_streams[ip];
    reliable_stream.template send(write_to_buffer);
  }

private:
  Network &m_network;
  std::map<IpAddress, ReliableStream> m_reliable_streams{};
};

}

#endif //NEPTUN_NEPTUN_NEPTUN_H
