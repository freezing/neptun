//
// Created by freezing on 13/03/2022.
//

#include <gtest/gtest.h>

#include "packet.h"

using namespace freezing::network;

namespace {

constexpr std::uint32_t kPacketId = 13;
constexpr std::uint32_t kAckSequenceNumber = 11;
constexpr std::uint32_t kAckBitmask = 0b10101100;

}

TEST(PacketTest, WriteAndRead) {
  std::vector<std::uint8_t> buffer(1600);
  auto payload = PacketHeader::write(buffer, kPacketId, kAckSequenceNumber, kAckBitmask);
  auto packet_header = PacketHeader(payload);

  std::vector<std::uint8_t> debug{};
  for (auto b : payload) {
    debug.push_back(b);
  }

  ASSERT_EQ(packet_header.id(), kPacketId);
  ASSERT_EQ(packet_header.ack_sequence_number(), kAckSequenceNumber);
  ASSERT_EQ(packet_header.ack_bitmask(), kAckBitmask);
}