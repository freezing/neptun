//
// Created by freezing on 26/03/2022.
//

#include <gtest/gtest.h>

#include "neptun/connection_manager.h"
#include "neptun/format.h"

using namespace freezing;
using namespace freezing::network;

namespace {

constexpr PacketId kPacketId = 33;
constexpr usize kNumRedundantPackets = 5;
constexpr BandwidthLimit kServerBandwidthLimit{120, 1400, 60, 800};
constexpr BandwidthLimit kClientBandwidthLimit{30, 400, 10, 300};
constexpr BandwidthLimit kInvalidBandwidthLimit{0, 0, 0, 0};

std::vector<u8> make_buffer(usize size = 1600) {
  return std::vector<u8>(size);
}

}

namespace freezing::network {

std::ostream &operator<<(std::ostream &os, const BandwidthLimit &bandwidth_limit) {
  return os << "BandwidthLimit(rate=" << (int) bandwidth_limit.max_send_packet_rate
            << ", max_packet_size="
            << bandwidth_limit.max_send_packet_size << ")";
}

}

TEST(ConnectionManagerTest, Handshake) {
  auto buffer = make_buffer();
  ConnectionManager server{ConnectionManagerConfig{kNumRedundantPackets, kServerBandwidthLimit}};
  ConnectionManager client{ConnectionManagerConfig{kNumRedundantPackets, kClientBandwidthLimit}};

  client.connect();
  {
    auto count = client.write(kPacketId, buffer);
    auto read_result = server.read(byte_span(buffer).first(count));
    ASSERT_TRUE(read_result.has_value());
    ASSERT_EQ(server.peer_limit(), std::optional{kClientBandwidthLimit});
  }

  {
    auto count = server.write(kPacketId, buffer);
    auto read_result = client.read(byte_span(buffer).first(count));
    client.on_packet_status_delivery(kPacketId, PacketDeliveryStatus::ACK);
    ASSERT_TRUE(read_result.has_value());
    ASSERT_EQ(client.peer_limit(), std::optional{kServerBandwidthLimit});
    ASSERT_TRUE(client.is_fully_connected()) << format_neptun_payload(byte_span(buffer).first(count));
  }

  {
    // Ensure server receives an ack and knows that it's fully connected.
    server.on_packet_status_delivery(kPacketId, PacketDeliveryStatus::ACK);
    ASSERT_TRUE(server.is_fully_connected());
  }
}

TEST(ConnectionManagerTest, InvalidBandwidthLimit) {
  auto buffer = make_buffer();
  ConnectionManager server{ConnectionManagerConfig{kNumRedundantPackets, kServerBandwidthLimit}};
  ConnectionManager client{ConnectionManagerConfig{kNumRedundantPackets, kInvalidBandwidthLimit}};

  client.connect();
  {
    auto count = client.write(kPacketId, buffer);
    auto read_result = server.read(byte_span(buffer).first(count));
    ASSERT_TRUE(read_result.has_value());
    ASSERT_EQ(server.peer_limit(), std::optional<BandwidthLimit>{});
    ASSERT_FALSE(server.is_peer_connected());
  }
}

TEST(ConnectionManagerTest, RedundantPackets) {
  auto buffer = make_buffer();
  ConnectionManager server{ConnectionManagerConfig{kNumRedundantPackets, kServerBandwidthLimit}};
  ConnectionManager client{ConnectionManagerConfig{kNumRedundantPackets, kClientBandwidthLimit}};

  client.connect();
  {
    for (usize i = 0; i < kNumRedundantPackets; i++) {
      // Ignore redundant packets.
      auto count = client.write(kPacketId + i, buffer);
      ASSERT_GT(count, 0);
    }
    // Process last one.
    auto count = client.write(kPacketId + kNumRedundantPackets, buffer);
    ASSERT_GT(count, 0);
    auto read_result = server.read(byte_span(buffer).first(count));
    ASSERT_TRUE(read_result.has_value());
    ASSERT_EQ(server.peer_limit(), std::optional{kClientBandwidthLimit});
  }
}

TEST(ConnectionManagerTest, ResendsDroppedLetsConnect) {
  auto buffer = make_buffer();
  ConnectionManager server{ConnectionManagerConfig{kNumRedundantPackets, kServerBandwidthLimit}};
  ConnectionManager client{ConnectionManagerConfig{kNumRedundantPackets, kClientBandwidthLimit}};

  client.connect();

  client.write(kPacketId, buffer);
  client.on_packet_status_delivery(kPacketId, PacketDeliveryStatus::DROP);
  auto count = client.write(kPacketId + 1, buffer);
  auto read_result = server.read(byte_span(buffer).first(count));
  ASSERT_TRUE(read_result.has_value());
  ASSERT_EQ(server.peer_limit(), std::optional{kClientBandwidthLimit});
}

TEST(ConnectionManagerTest, DoesNothingIfNoPacketDeliveryStatus) {
  auto buffer = make_buffer();
  ConnectionManager server{ConnectionManagerConfig{kNumRedundantPackets, kServerBandwidthLimit}};
  ConnectionManager
      client{ConnectionManagerConfig{0 /* num_redundant_packets */, kClientBandwidthLimit}};

  client.connect();

  {
    auto count = client.write(kPacketId, buffer);
    auto read_result = server.read(byte_span(buffer).first(count));
    ASSERT_TRUE(read_result.has_value());
    ASSERT_EQ(server.peer_limit(), std::optional{kClientBandwidthLimit});
  }

  {
    auto count = client.write(kPacketId + 1, buffer);
    ASSERT_EQ(count, 0);
  }
}