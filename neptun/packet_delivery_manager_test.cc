//
// Created by freezing on 15/03/2022.
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "common/types.h"
#include "neptun/packet_delivery_manager.h"

using namespace freezing;
using namespace freezing::network;
using namespace testing;

namespace {

constexpr u32 kPacketId = 3;
// 2022-03-15 21:18:41 GMT (not relevant, but doesn't hurt to know).
constexpr u64 kNow = 1647379116;

std::vector<u8> make_buffer(usize capacity = 1600) {
  return std::vector<u8>(capacity);
}

}

namespace freezing::network {

std::ostream &operator<<(std::ostream &stream, const PacketDeliveryStatus &status) {
  switch (status) {
  case PacketDeliveryStatus::ACK:return stream << "ACK";
    break;
  case PacketDeliveryStatus::DROP:return stream << "DROP";
    break;
  default:throw std::runtime_error("Unknown PacketDeliveryStatus");
  }
}

}

TEST(PacketDeliveryManagerTest, SentPacketIdsAreContinuouslyIncreasing) {
  constexpr u32 kInitialExpectedPacketId = 10;
  auto buffer = make_buffer();
  PacketDeliveryManager manager{kInitialExpectedPacketId};

  for (u32 expected_packet_id = 0; expected_packet_id < 100; expected_packet_id++) {
    auto byte_count = manager.write(buffer, kNow);
    ASSERT_EQ(byte_count, PacketHeader::kSerializedSize);
    auto header = PacketHeader(buffer);
    ASSERT_EQ(header.id(), expected_packet_id);
  }
}

TEST(PacketDeliveryManagerTest, AcksAndDropsSentPackets) {
  // Irrelevant for the test.
  constexpr u32 kInitialExpectedPacketId = 10;
  PacketDeliveryManager manager{kInitialExpectedPacketId};

  // Send 30 packets.
  for (u32 packet_id = 0; packet_id < 30; packet_id++) {
    auto write_buffer = make_buffer();
    manager.write(write_buffer, kNow);
  }

  // Read a acks from the packet that's been sent by the peer.
  auto read_buffer = make_buffer();
  constexpr u32 kAckSequenceNumber = 15;
  // Packets 15(15+0), 16(15+1), 20(15+5), 23(15+8) and 25(15+10) are acked.
  constexpr u32 kAckBitmask = (1 << 0) | (1 << 1) | (1 << 5) | (1 << 8) | (1 << 10);
  PacketHeader::write(read_buffer, kPacketId, kAckSequenceNumber, kAckBitmask);
  auto[read_count, delivery_statuses, packet_id] = manager.process_read(read_buffer);
  // It doesn't matter if we were able to read the packet(returned [read_count = 0],
  // e.g. if it's too old. We still process the acks.
  ASSERT_THAT(delivery_statuses.to_vector(), ElementsAre(
      std::make_pair(0, PacketDeliveryStatus::DROP),
      std::make_pair(1, PacketDeliveryStatus::DROP),
      std::make_pair(2, PacketDeliveryStatus::DROP),
      std::make_pair(3, PacketDeliveryStatus::DROP),
      std::make_pair(4, PacketDeliveryStatus::DROP),
      std::make_pair(5, PacketDeliveryStatus::DROP),
      std::make_pair(6, PacketDeliveryStatus::DROP),
      std::make_pair(7, PacketDeliveryStatus::DROP),
      std::make_pair(8, PacketDeliveryStatus::DROP),
      std::make_pair(9, PacketDeliveryStatus::DROP),
      std::make_pair(10, PacketDeliveryStatus::DROP),
      std::make_pair(11, PacketDeliveryStatus::DROP),
      std::make_pair(12, PacketDeliveryStatus::DROP),
      std::make_pair(13, PacketDeliveryStatus::DROP),
      std::make_pair(14, PacketDeliveryStatus::DROP),
      std::make_pair(15, PacketDeliveryStatus::ACK),
      std::make_pair(16, PacketDeliveryStatus::ACK),
      std::make_pair(17, PacketDeliveryStatus::DROP),
      std::make_pair(18, PacketDeliveryStatus::DROP),
      std::make_pair(19, PacketDeliveryStatus::DROP),
      std::make_pair(20, PacketDeliveryStatus::ACK),
      std::make_pair(21, PacketDeliveryStatus::DROP),
      std::make_pair(22, PacketDeliveryStatus::DROP),
      std::make_pair(23, PacketDeliveryStatus::ACK),
      std::make_pair(24, PacketDeliveryStatus::DROP),
      std::make_pair(25, PacketDeliveryStatus::ACK)
  ));
}

TEST(PacketDeliveryManagerTest, BoundaryAck) {
  // Irrelevant for the test.
  constexpr u32 kInitialExpectedPacketId = 10;
  PacketDeliveryManager manager{kInitialExpectedPacketId};

  // Send 34 packets.
  for (u32 packet_id = 0; packet_id < 34; packet_id++) {
    auto write_buffer = make_buffer();
    manager.write(write_buffer, kNow);
  }

  // Ack only the last packet.
  auto read_buffer = make_buffer();
  constexpr u32 kAckSequenceNumber = 0;
  // Packets 32(0+32).
  constexpr u32 kAckBitmask = (1 << 31);
  PacketHeader::write(read_buffer, kPacketId, kAckSequenceNumber, kAckBitmask);
  auto[read_count, delivery_statuses, packet_id] = manager.process_read(read_buffer);

  ASSERT_THAT(delivery_statuses.to_vector(), ElementsAre(
      std::make_pair(0, PacketDeliveryStatus::DROP),
      std::make_pair(1, PacketDeliveryStatus::DROP),
      std::make_pair(2, PacketDeliveryStatus::DROP),
      std::make_pair(3, PacketDeliveryStatus::DROP),
      std::make_pair(4, PacketDeliveryStatus::DROP),
      std::make_pair(5, PacketDeliveryStatus::DROP),
      std::make_pair(6, PacketDeliveryStatus::DROP),
      std::make_pair(7, PacketDeliveryStatus::DROP),
      std::make_pair(8, PacketDeliveryStatus::DROP),
      std::make_pair(9, PacketDeliveryStatus::DROP),
      std::make_pair(10, PacketDeliveryStatus::DROP),
      std::make_pair(11, PacketDeliveryStatus::DROP),
      std::make_pair(12, PacketDeliveryStatus::DROP),
      std::make_pair(13, PacketDeliveryStatus::DROP),
      std::make_pair(14, PacketDeliveryStatus::DROP),
      std::make_pair(15, PacketDeliveryStatus::DROP),
      std::make_pair(16, PacketDeliveryStatus::DROP),
      std::make_pair(17, PacketDeliveryStatus::DROP),
      std::make_pair(18, PacketDeliveryStatus::DROP),
      std::make_pair(19, PacketDeliveryStatus::DROP),
      std::make_pair(20, PacketDeliveryStatus::DROP),
      std::make_pair(21, PacketDeliveryStatus::DROP),
      std::make_pair(22, PacketDeliveryStatus::DROP),
      std::make_pair(23, PacketDeliveryStatus::DROP),
      std::make_pair(24, PacketDeliveryStatus::DROP),
      std::make_pair(25, PacketDeliveryStatus::DROP),
      std::make_pair(26, PacketDeliveryStatus::DROP),
      std::make_pair(27, PacketDeliveryStatus::DROP),
      std::make_pair(28, PacketDeliveryStatus::DROP),
      std::make_pair(29, PacketDeliveryStatus::DROP),
      std::make_pair(30, PacketDeliveryStatus::DROP),
      std::make_pair(31, PacketDeliveryStatus::ACK)
  ));
}

TEST(PacketDeliveryManagerTest, PacketsAreDroppedIfNotAckedForSomeTime) {
  // Irrelevant for the test.
  constexpr u32 kInitialExpectedPacketId = 10;
  constexpr u32 kPacketTimeoutSeconds = 5;
  PacketDeliveryManager manager{kInitialExpectedPacketId, kPacketTimeoutSeconds};

  // Send 34 packets.
  for (u32 packet_id = 0; packet_id < 34; packet_id++) {
    auto write_buffer = make_buffer();
    manager.write(write_buffer, kNow + packet_id);
  }

  // 10 seconds have passed, so packets at time[now + 0, now + 5] have expired.
  constexpr u32 kNewNow = kNow + 10;
  auto delivery_statuses = manager.drop_old_packets(kNewNow);

  ASSERT_THAT(delivery_statuses.to_vector(), ElementsAre(
      std::make_pair(0, PacketDeliveryStatus::DROP),
      std::make_pair(1, PacketDeliveryStatus::DROP),
      std::make_pair(2, PacketDeliveryStatus::DROP),
      std::make_pair(3, PacketDeliveryStatus::DROP),
      std::make_pair(4, PacketDeliveryStatus::DROP),
      std::make_pair(5, PacketDeliveryStatus::DROP)));
}

TEST(PacketDeliveryManagerTest, ReadsExpectedPackets) {
  constexpr u32 kExpectedPacketId = 10;
  PacketDeliveryManager manager{kExpectedPacketId};

  for (u32 packet_id = 10; packet_id < 20; packet_id++) {
    auto buffer = make_buffer();
    // Acks are irrelevant for this test.
    auto ack_sequence_number = 0;
    auto ack_bitmask = 0;
    PacketHeader::write(buffer, packet_id, ack_sequence_number, ack_bitmask);
    auto[read_byte_count, delivery_status_ignored, actual_packet_id] = manager.process_read(buffer);
    ASSERT_EQ(read_byte_count, PacketHeader::kSerializedSize);
    ASSERT_EQ(actual_packet_id, packet_id);
  }
}

TEST(PacketDeliveryManagerTest, ReadsPacketsAfterExpected) {
  constexpr u32 kExpectedPacketId = 10;
  PacketDeliveryManager manager{kExpectedPacketId};

  auto buffer = make_buffer();
  constexpr u32 packet_id = 15;
  // Acks are irrelevant for this test.
  auto ack_sequence_number = 0;
  auto ack_bitmask = 0;
  PacketHeader::write(buffer, packet_id, ack_sequence_number, ack_bitmask);
  auto[read_byte_count, delivery_status_ignored, actual_packet_id] = manager.process_read(buffer);
  ASSERT_EQ(read_byte_count, PacketHeader::kSerializedSize);
  ASSERT_EQ(actual_packet_id, packet_id);
}

TEST(PacketDeliveryManagerTest, IgnoresPacketsBeforeExpected) {
  constexpr u32 kExpectedPacketId = 10;
  PacketDeliveryManager manager{kExpectedPacketId};

  auto buffer = make_buffer();
  constexpr u32 packet_id = 9;
  // Acks are irrelevant for this test.
  auto ack_sequence_number = 0;
  auto ack_bitmask = 0;
  PacketHeader::write(buffer, packet_id, ack_sequence_number, ack_bitmask);
  auto[read_byte_count, delivery_status_ignored, actual_packet_id] = manager.process_read(buffer);
  ASSERT_EQ(read_byte_count, 0);
  ASSERT_EQ(packet_id, actual_packet_id);
}
