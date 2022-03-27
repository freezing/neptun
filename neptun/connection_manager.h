//
// Created by freezing on 26/03/2022.
//

#ifndef NEPTUN_NEPTUN_CONNECTION_MANAGER_H
#define NEPTUN_NEPTUN_CONNECTION_MANAGER_H

#include <queue>

#include "common/types.h"
#include "network/io_buffer.h"
#include "neptun/error.h"
#include "neptun/common.h"
#include "neptun/messages/segment.h"
#include "neptun/messages/message_header.h"
#include "neptun/messages/lets_connect.h"
#include "neptun/messages/reject_lets_connect.h"

namespace freezing::network {

struct BandwidthLimit {
  u8 rate;
  // Maximum size of the packet in bytes.
  u16 max_packet_size;

  bool operator <=>(const BandwidthLimit&) const = default;
};

// Responsible for establishing and maintaining the connection.
// The connection manager determines the packet rate that the peer can send and receive, as well
// as the maximum size of each packet (in bytes).
// Additionally, the connection manager ensures that the packet belongs to our game's protocol.
// Other managers do not exchange messages until the handshake is complete.
//
// The protocol works as follows (using server/client peers to distinguish between the
// peer accepting the connection and the peer requesting to connect, even though both server and
// client peers are equal at the connection layer):
//   0) The client peer sends the LetsConnect message.
//   1) The server peer receives the LetsConnect message and decides whether it's okay to establish
//      the connection with the client peer.
//      The request specifies the maximum bandwidth capacity the client peer can handle.
//   2) The server peer responds with the LetsConnect message and includes its own bandwidth
//      limits.
//      If the connection should be rejected, the server responds with RejectLetsConnect.
//   3) At some point in the future, any peer may gracefully disconnect from the other by sending
//      the Bye message. The Bye message is not required, e.g. a peer may crash, so this is only
//      best-effort. Therefore, the receiving peer doesn't respond to it.
//
// The connection manager ensures that the bandwidth limits to both sides are respected.
// If the peer sends packets with frequency or size that violate the bandwidth limits, it is
// disconnected. From the ConnectionManager's perspective, disconnecting a client means letting the
// higher-level known.
// ConnectionManager exposes an API for Neptun to check if the connection has been established.
//
// Packet encoding details:
// ConnectionManager's messages are preceded by the CONNECTION_MANAGER segment in each packet.
// Peers do not include any other stream messages in the packet before the connection has been
// established. If so, the peer is malicious.
//
// Implementation details:
// Since the connection manager uses UDP, it can't assume that the messages are delivered.
// Therefore, it sends up to N packets to establish the connection and N packets to respond.
// Sending more than one packet increases the likelihood of at least one packet being received
// by the peer.
// Good choice of N depends on the packet loss, but N=5 should be sufficient for most connections.
// Any messages related to the initial handshake are ignored if the handshake has already happened,
// so it's safe to send as many LetsConnect and OkayLetsConnect messages.
// Only the first (received, which is not necessarily the same as sent) LetsConnect
// and OkayLetsConnect messages are processed, so they should have the same config
// (bandwidth limits). For clarity, this is enforced in the protocol and the peer not respecting
// this is malicious.
//
// If any of the packets with the handshake messages is dropped, the peer re-sends them in the
// next packet, unless the handshake has been completed.
//
// TODO: This should be handled by TimeoutHandler.
// It is still possible that none of the packets are received. If the connection doesn't see any
// packets from the peer for some time (max_allowed_time_since_last_packet),
// the peer is disconnected.
// This is true even after successfully establishing the connection.
//
// Future ideas:
// It would be interesting to have a better support for bad connections or connections that may
// be lost for some time period without requiring them to reconnect and sync the full state again.
// This could be useful for games on mobile phones, e.g. if the player enters the tunnel and loses
// the connection for 1 minute we may want to avoid them having to reconnect and sync the full
// state again.
class ConnectionManager {
public:
  explicit ConnectionManager(usize num_redundant_packets, BandwidthLimit bandwidth_limit)
      : m_num_redundant_packets{num_redundant_packets}, m_bandwidth_limit{bandwidth_limit} {}

  // Next time write() function is called it will include LetsConnect message.
  void connect() {
    m_num_lets_connect_to_send = m_num_redundant_packets + 1;
    m_is_initiator = true;
  }

  void on_packet_status_delivery(PacketId packet_id, PacketDeliveryStatus status) {
    switch (status) {
    case PacketDeliveryStatus::ACK: {
      if (!m_in_flight_lets_connect.empty() && m_in_flight_lets_connect.front() == packet_id) {
        m_in_flight_lets_connect.pop();
      }
      break;
    }
    case PacketDeliveryStatus::DROP: {
      if (!m_in_flight_lets_connect.empty() && m_in_flight_lets_connect.front() == packet_id) {
        m_in_flight_lets_connect.pop();
        if (!m_peer_bandwidth_limit.has_value()) {
          m_num_lets_connect_to_send++;
        }
      }
      break;
    }
    }
  }

  // TODO: Consider decoupling Reader from Writer. The only problem is if they need to share
  // some state, but even that can be done (just not sure if the overall result is better).
  expected<usize, NeptunError> read(byte_span buffer) {
    // TODO: Read Segment if possible.
    // If it's a CONNECTION_MANAGER segment, then read ConnectionManager messages.
    // Each message is preceded with a message type which is [u16].
    if (buffer.size() < Segment::kSerializedSize) {
      return {0};
    }
    Segment segment(buffer);
    if (segment.manager_type() != ManagerType::CONNECTION_MANAGER) {
      return {0};
    }
    // Peer may send exactly one message per connection manager segment.
    if (segment.message_count() != 1) {
      return make_error(NeptunError::MALFORMED_PACKET);
    }

    usize idx = Segment::kSerializedSize;
    MessageHeader message_header(advance(buffer, idx));
    idx += MessageHeader::kSerializedSize;

    switch (message_header.message_type()) {
    case LetsConnect::kId: {
      LetsConnect lets_connect(advance(buffer, idx));
      if (validate(lets_connect)) {
        // If we see any invalid requests, all future requests are ignored.
        is_fail = false;
        if (!m_is_initiator) {
          // This is a request.
          m_num_lets_connect_to_send = m_num_redundant_packets;
          set_peer_bandwidth_limit(lets_connect);
        } else {
          // This is a response. Ignore any future requests.
          // Clear the queue.
          while (!m_in_flight_lets_connect.empty()) {
            m_in_flight_lets_connect.pop();
          }
          set_peer_bandwidth_limit(lets_connect);
        }
      } else {
        is_fail = true;
      }
      return idx + LetsConnect::kSerializedSize;
    }
    case RejectLetsConnect::kId:
      return make_error(NeptunError::LETS_CONNECT_REJECTED);
    default:return make_error(NeptunError::MALFORMED_PACKET);
    }
  }

  usize write(PacketId packet_id, byte_span buffer) {
    if (m_num_lets_connect_to_send > 0) {
      // TODO: Handle buffer not big enough.
      auto payload =
          Segment::write(buffer,
                         ManagerType::CONNECTION_MANAGER,
                         1);
      usize idx = payload.size();
      IoBuffer io{buffer};
      if (is_fail) {
        MessageHeader::write(advance(buffer, idx), LetsConnect::kId);
        RejectLetsConnect::write(advance(buffer, idx + MessageHeader::kSerializedSize));
        idx += MessageHeader::kSerializedSize + RejectLetsConnect::kSerializedSize;
      } else {
        MessageHeader::write(advance(buffer, idx), LetsConnect::kId);
        LetsConnect::write(advance(buffer, idx + MessageHeader::kSerializedSize),
                           m_bandwidth_limit.rate,
                           m_bandwidth_limit.max_packet_size);
        idx += MessageHeader::kSerializedSize + LetsConnect::kSerializedSize;
      }
      m_in_flight_lets_connect.push(packet_id);
      assert(idx < buffer.size());
      m_num_lets_connect_to_send--;
      return idx;
    } else {
      return 0;
    }
  }

  std::optional<BandwidthLimit> peer_limit() const {
    return m_peer_bandwidth_limit;
  }

private:
  usize m_num_redundant_packets;
  BandwidthLimit m_bandwidth_limit;
  std::queue<PacketId> m_in_flight_lets_connect{};
  usize m_num_lets_connect_to_send{0};
  bool is_fail{false};
  bool m_is_initiator{false};
  std::optional<BandwidthLimit> m_peer_bandwidth_limit{};

  bool validate(LetsConnect lets_connect) {
    // There's nothing to validate. I may decide to remove this if it's not needed.
    return true;
  }

  void set_peer_bandwidth_limit(LetsConnect lets_connect) {
    m_peer_bandwidth_limit =
        BandwidthLimit{lets_connect.packet_rate(), lets_connect.max_packet_length()};
  }
};

}

#endif //NEPTUN_NEPTUN_CONNECTION_MANAGER_H
