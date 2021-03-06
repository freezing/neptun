//
// Created by freezing on 15/03/2022.
//

#ifndef NEPTUN_NEPTUN_PACKET_DELIVERY_MANAGER_H
#define NEPTUN_NEPTUN_PACKET_DELIVERY_MANAGER_H

#include <algorithm>
#include <tuple>
#include <queue>

#include "neptun/messages/packet_header.h"
#include "neptun/common.h"

// TODO: Use PacketId instead of u32 for packet id.
namespace freezing::network {

namespace detail {

static const seconds kDefaultPacketTimeout = seconds(5);

template<typename Clock>
struct InFlightPacket {
  u32 id;
  time_point<Clock> time_dispatched;
};

static std::optional<u32> most_significant_bit(u32 value) {
  std::optional<u32> msb{};
  for (u32 bit = 0; bit < sizeof(u32) * 8; bit++) {
    u32 mask = 1 << bit;
    if ((mask & value) > 0) {
      msb = bit;
    }
  }
  return msb;
}

}

class DeliveryStatuses {
public:
  void add_ack(PacketId packet_id) {
    m_statuses.emplace_back(packet_id, PacketDeliveryStatus::ACK);
  }

  void add_drop(PacketId packet_id) {
    m_statuses.emplace_back(packet_id, PacketDeliveryStatus::DROP);
  }

  // TODO: Implement iterator.
  template<typename Fn>
  void for_each(Fn fn) const {
    for (auto[packet_id, status] : m_statuses) {
      fn(packet_id, status);
    }
  }

  std::vector<std::pair<PacketId, PacketDeliveryStatus>> to_vector() const {
    return m_statuses;
  }

private:
  std::vector<std::pair<PacketId, PacketDeliveryStatus>> m_statuses;

};

template<typename Clock>
class PacketDeliveryManager {
public:
  explicit PacketDeliveryManager(
      PacketId next_expected_packet_id,
      milliseconds packet_timeout = detail::kDefaultPacketTimeout) : m_next_expected_packet_id{
      next_expected_packet_id}, m_packet_timeout{
      packet_timeout} {}

  // TODO: API should be clearer. I get confused by what is what.
  // If the returned usize is 0, then the packet should not be processed.
  // It's either a duplicate or it is assumed to be dropped.
  std::tuple<usize, DeliveryStatuses, PacketId> process_read(byte_span buffer) {
    auto header = PacketHeader(buffer);
    DeliveryStatuses statuses = process_acks(header.ack_sequence_number(), header.ack_bitmask());
    usize processed_byte_count = process_packet_header(header, buffer);
    return {processed_byte_count, statuses, header.id()};
  }

  DeliveryStatuses drop_old_packets(time_point<Clock> now) {
    DeliveryStatuses statuses{};
    while (!m_in_flight_packets.empty()) {
      auto in_flight = m_in_flight_packets.front();
      if (in_flight.time_dispatched + m_packet_timeout <= now) {
        statuses.add_drop(in_flight.id);
        m_in_flight_packets.pop();
      } else {
        break;
      }
    }
    return statuses;
  }

  usize write(byte_span buffer, time_point<Clock> now) {
    auto packet_id = m_next_outgoing_packet_id++;
    m_in_flight_packets.push({packet_id, now});
    if (m_pending_acks.empty()) {
      auto ack_sequence_number = 0;
      auto ack_bitmask = 0;
      auto write_count =
          PacketHeader::write(buffer, packet_id, ack_sequence_number, ack_bitmask).size();
      assert(write_count == PacketHeader::kSerializedSize);
      return write_count;
    } else {
      auto ack_sequence_number = m_pending_acks.front();
      auto ack_bitmask = 0;
      while (!m_pending_acks.empty()) {
        auto pending_packet_id_ack = m_pending_acks.front();
        // TODO: A helper function to check if [ack] can be described for [ack_sequence_number].
        assert(pending_packet_id_ack >= ack_sequence_number);
        auto bit_position = pending_packet_id_ack - ack_sequence_number;
        if (bit_position >= sizeof(u32) * 8) {
          // If ack can't fit in the bitmask, it must be sent via some future packet.
          break;
        } else {
          ack_bitmask = ack_bitmask | (1 << bit_position);
        }
        m_pending_acks.pop();
      }
      PacketHeader::write(buffer, packet_id, ack_sequence_number, ack_bitmask);
      return PacketHeader::kSerializedSize;
    }
  }

private:
  milliseconds m_packet_timeout;
  u32 m_next_outgoing_packet_id{0};
  u32 m_next_expected_packet_id;
  std::queue<u32> m_pending_acks{};
  std::queue<detail::InFlightPacket<Clock>> m_in_flight_packets{};

  DeliveryStatuses process_acks(u32 ack_sequence_number, u32 ack_bitmask) {
    // All 0 after the highest set bit are ignored because it's possible that the other host
    // hasn't received the corresponding packets yet.
    auto msb = detail::most_significant_bit(ack_bitmask);
    if (!msb) {
      // All bits are 0.
      return DeliveryStatuses{};
    } else {
      u32 highest_acked_packet_id = ack_sequence_number + *msb;
      DeliveryStatuses statuses{};
      while (!m_in_flight_packets.empty()) {
        auto in_flight_packet = m_in_flight_packets.front();

        if (in_flight_packet.id > highest_acked_packet_id) {
          // We are yet to receive any information about the in-flight packets.
          break;
        } else {
          if (in_flight_packet.id < ack_sequence_number) {
            // If in-flight packet is too far in the past, drop it.
            statuses.add_drop(in_flight_packet.id);
          } else {
            u32 delta = in_flight_packet.id - ack_sequence_number;
            assert(delta <= sizeof(u32) * 8);
            u32 in_flight_packet_bitmask = 1 << delta;
            bool is_in_flight_packet_acked = (in_flight_packet_bitmask & ack_bitmask) > 0;
            if (is_in_flight_packet_acked) {
              statuses.add_ack(in_flight_packet.id);
            } else {
              statuses.add_drop(in_flight_packet.id);
            }
          }
          // Packet is not in-flight anymore if [in_flight_packet.id <= ack_sequence_number].
          m_in_flight_packets.pop();
        }
      }
      return statuses;
    }
  }

  usize process_packet_header(const PacketHeader &header, byte_span buffer) {
    if (header.id() == m_next_expected_packet_id) {
      add_pending_ack(header.id());
      m_next_expected_packet_id = header.id() + 1;
      return PacketHeader::kSerializedSize;
    } else if (header.id() < m_next_expected_packet_id) {
      // The packet is too old. If it's a duplicate, then we have already processed it.
      // If not, we drop it!
      return 0;
    } else {
      assert(header.id() > m_next_expected_packet_id);
      // Even though we may receive some packets with lower ids in the future, i.e. from range
      // [m_next_expected_packet_id, header.id()), we treat them as dropped because we are not
      // going to wait for them.
      add_pending_ack(header.id());
      m_next_expected_packet_id = header.id() + 1;
      return PacketHeader::kSerializedSize;
    }
  }

  void add_pending_ack(PacketId packet_id) {
    m_pending_acks.push(packet_id);
  }
};

}

#endif //NEPTUN_NEPTUN_PACKET_DELIVERY_MANAGER_H
