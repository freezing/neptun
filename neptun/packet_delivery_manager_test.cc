//
// Created by freezing on 15/03/2022.
//

#include <gtest/gtest.h>

#include "common/types.h"
#include "neptun/packet_delivery_manager.h"

using namespace freezing;
using namespace freezing::network;

namespace {

constexpr u32 kInitialExpectedPacketId = 10;
// 2022-03-15 21:18:41 GMT (not relevant, but doesn't hurt to know).
constexpr u64 kNow = 1647379116;

std::vector<u8> make_buffer(usize capacity = 1600) {
  return std::vector<u8>(capacity);
}

}

TEST(PacketDeliveryManagerTest, SentPacketsAreUnique) {
  auto buffer = make_buffer();
  PacketDeliveryManager manager{kInitialExpectedPacketId};

  for (u32 expected_packet_id = 0; expected_packet_id < 100; expected_packet_id++) {
    auto byte_count = manager.write(buffer, kNow);
    ASSERT_EQ(byte_count, PacketHeader::kSerializedSize);
    auto header = PacketHeader(buffer);
    ASSERT_EQ(header.id(), expected_packet_id);
  }
}

// TODO: Test that packets timeout after some time.