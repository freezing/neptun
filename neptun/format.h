//
// Created by freezing on 27/03/2022.
//

#ifndef NEPTUN_NEPTUN_FORMAT_H
#define NEPTUN_NEPTUN_FORMAT_H

#include <sstream>
#include <string>

#include "common/types.h"
#include "neptun/messages/message_header.h"
#include "neptun/messages/lets_connect.h"
#include "neptun/messages/reject_lets_connect.h"
#include "neptun/messages/segment.h"
#include "neptun/messages/reliable_message.h"
#include "neptun/messages/packet_header.h"
#include "neptun/messages/unreliable_message.h"

namespace freezing::network {

inline std::string to_hex(byte_span payload) {
  std::stringstream ss;
  std::string sep{};
  for (auto byte : payload) {
    ss << sep << std::hex << (int) byte;
    sep = " ";
  }
  return ss.str();
};

inline std::string format_neptun_payload(byte_span payload) {
  // TODO: Header must have protocol_id to allow us to identify neptun packets.
  auto packet_header = PacketHeader(payload);
  payload = advance(payload, PacketHeader::kSerializedSize);
  std::stringstream ss;
  ss << "[packet_id=" << packet_header.id() << ", ack_seq_num="
     << packet_header.ack_sequence_number() << ", ack_bitmask=" << packet_header.ack_bitmask()
     << "]";
  ss << " ";

  auto append_segment = [&ss, &payload](const Segment &segment) {
    auto format_manager_type = [](u8 manager_type) {
      switch (manager_type) {
        case ManagerType::CONNECTION_MANAGER:
          return "ConnectionManager";
        case ManagerType::RELIABLE_STREAM:
          return "Reliable";
        case ManagerType::UNRELIABLE_STREAM:
          return "Unreliable";
        default:
          return "Unknown";
      }
    };

    auto append_connection_manager_segment = [&ss, &payload]() {
      MessageHeader header(payload);
      payload = advance(payload, MessageHeader::kSerializedSize);
      switch (header.message_type()) {
        case LetsConnect::kId: {
          auto lets_connect = LetsConnect(payload);
          payload = advance(payload, LetsConnect::kSerializedSize);
          ss << "[max_read_packet_rate=" << (int) lets_connect.max_read_packet_rate()
             << ", max_read_packet_size="
             << lets_connect.max_read_packet_size() << ", max_send_packet_rate="
             << (int) lets_connect.max_send_packet_rate() << ", max_send_packet_size="
             << lets_connect.max_send_packet_size() << "]";
          return;
        }
        case RejectLetsConnect::kId: {
          auto reject_lets_connect = RejectLetsConnect(payload);

          ss << "[RejectLetsConnect]";
          return;
        }
      }
    };

    auto append_reliable_segment = [&ss, &payload](usize msg_count) {
      for (usize idx = 0; idx < msg_count; idx++) {
        auto msg = ReliableMessage(payload);
        payload = advance(payload, msg.serialized_size());
        ss << "[seq_num=" << msg.sequence_number() << ", length=" << msg.length() << ", payload="
           << to_hex(msg.payload()) << "]";
      }
    };

    auto append_unreliable_segment = [&ss, &payload](usize msg_count) {
      for (usize idx = 0; idx < msg_count; idx++) {
        auto msg = UnreliableMessage(payload);
        payload = advance(payload, msg.serialized_size());
        ss << "[length=" << msg.length() << ", payload=" << to_hex(msg.payload()) << "]";
      }
    };

    // TODO: Group Segment and ReliableMessages into ReliableSegment message.
    ss << "[" << format_manager_type(segment.manager_type()) << ", msg_count="
       << std::to_string(segment.message_count()) << "]";
    payload = advance(payload, Segment::kSerializedSize);

    switch (segment.manager_type()) {
      case ManagerType::CONNECTION_MANAGER:
        assert(segment.message_count() == 1);
        append_connection_manager_segment();
        break;
      case ManagerType::RELIABLE_STREAM:
        append_reliable_segment(segment.message_count());
        break;
      case ManagerType::UNRELIABLE_STREAM:
        append_unreliable_segment(segment.message_count());
        break;
      default:
        throw std::runtime_error("Unknown manager type: " + std::to_string(segment.manager_type()));
    }
  };

  std::string sep{};
  while (!payload.empty()) {
    ss << sep;
    auto segment = Segment(payload);
    append_segment(segment);
    sep = " ";
  }
  return ss.str();
}

}

#endif //NEPTUN_NEPTUN_FORMAT_H
