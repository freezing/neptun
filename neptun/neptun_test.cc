#include <gtest/gtest.h>

#include "common/types.h"
#include "common/fake_clock.h"
#include "network/fake_network.h"
#include "neptun/neptun.h"
#include "neptun/format.h"

using namespace freezing;
using namespace freezing::network;
using namespace freezing::network::testing;

namespace {
const IpAddress kServerIp = IpAddress::from_ipv4("192.168.0.10", 1000);
const IpAddress kClientIp = IpAddress::from_ipv4("192.168.0.11", 2000);
constexpr ConnectionManagerConfig kConnectionManagerConfig{5,
                                                           BandwidthLimit{.max_read_packet_rate=120, .max_read_packet_size=1400, .max_send_packet_rate=60, .max_send_packet_size=800}};
const FakeClock::time_point kNow = FakeClock::now();

const auto unexpected_reliable_msgs = [](byte_span buffer) { FAIL(); };
}

using TestNeptun = Neptun<FakeNetwork, FakeClock>;

void connect(TestNeptun &server, TestNeptun &client, FakeNetwork &fake_network) {
  client.connect(kServerIp);
  client.tick(kNow);
  server.tick(kNow);
  // Process server's response. But do not send any packets.
  fake_network.drop_packets(true);
  client.tick(kNow);
  fake_network.drop_packets(false);
}

TEST(NeptunTest, ConnectionHandshake) {
  FakeNetwork fake_network{};
  TestNeptun server{fake_network, kServerIp, kConnectionManagerConfig};
  TestNeptun client{fake_network, kClientIp, kConnectionManagerConfig};
  connect(server, client, fake_network);
  ASSERT_TRUE(server.is_connected(kClientIp));
  ASSERT_TRUE(client.is_connected(kServerIp));
}

TEST(NeptunTest, PacketRateLimit) {
  FakeNetwork fake_network{};
  auto server_limit = BandwidthLimit{
      .max_read_packet_rate = 120,
      .max_read_packet_size = 1400,
      .max_send_packet_rate = 90,
      .max_send_packet_size = 1400,
  };
  auto client_limit = BandwidthLimit{
      .max_read_packet_rate = 60,
      .max_read_packet_size = 800,
      // We pretend that the upload speed is smaller on the client.
      .max_send_packet_rate = 30,
      .max_send_packet_size = 400,
  };
  TestNeptun server{fake_network, kServerIp, ConnectionManagerConfig{0, server_limit}};
  TestNeptun client{fake_network, kClientIp, ConnectionManagerConfig{0, client_limit}};
  connect(server, client, fake_network);

  // Let's call tick once every millisecond, which results in 1000 calls.
  // We expect:
  //   - Server to receive 60 packets (not 120, because client can send maximum 60 packets a second)
  //   - Client to receive 30 packets
  for (usize ms = 0; ms < 1000; ms++) {
    auto now = kNow + milliseconds(ms);
    client.tick(now);
    server.tick(now);
  }

  auto server_stats = fake_network.stats(kServerIp);
  auto client_stats = fake_network.stats(kClientIp);
  ASSERT_EQ(server_stats.num_sent_packets, 30);
  ASSERT_EQ(client_stats.num_read_packets, 30);
  ASSERT_EQ(server_stats.num_read_packets, 120);
  ASSERT_EQ(client_stats.num_sent_packets, 120);
}

TEST(NeptunTest, PacketSizeLimit) {
  FakeNetwork fake_network{};
  auto server_limit = BandwidthLimit{
      .max_read_packet_rate = 120,
      .max_read_packet_size = 1400,
      .max_send_packet_rate = 90,
      .max_send_packet_size = 1400,
  };
  auto client_limit = BandwidthLimit{
    .max_read_packet_rate = 60,
    .max_read_packet_size = 800,
    // We pretend that the upload speed is smaller on the client.
    .max_send_packet_rate = 30,
    .max_send_packet_size = 400,
  };
  TestNeptun server{fake_network, kServerIp, ConnectionManagerConfig{0, server_limit}};
  TestNeptun client{fake_network, kClientIp, ConnectionManagerConfig{0, client_limit}};
  connect(server, client, fake_network);

  // Let's try to send too many reliable messages that can't fit in a single packet.
  // We expect:
  //   - Server to receive a packet of size ~1400 (a bit less)
  //   - Client to receive a packet of size ~400 (a bit less)
  constexpr usize kTooManyReliableMsgsCount = 1000;
  for (usize i = 0; i < kTooManyReliableMsgsCount; i++) {
    client.send_reliable_to(kServerIp, [i](byte_span buffer) {
      IoBuffer io(buffer);
      auto count = io.write_u16(i, 0);
      return buffer.first(count);
    });
    server.send_reliable_to(kClientIp, [i](byte_span buffer) {
      IoBuffer io(buffer);
      auto count = io.write_u16(i, 0);
      return buffer.first(count);
    });
  }
  client.tick(kNow);
  server.tick(kNow);

  auto server_stats = fake_network.stats(kServerIp);
  auto client_stats = fake_network.stats(kClientIp);
  ASSERT_EQ(server_stats.num_sent_bytes, 400);
  ASSERT_EQ(client_stats.num_read_bytes, 400);
  ASSERT_EQ(server_stats.num_read_bytes, 1400);
  ASSERT_EQ(client_stats.num_sent_bytes, 400);
}

TEST(NeptunTest, DropConnectionIfPacketLimitIsViolated) {
  FAIL();
}

TEST(NeptunTest, ReadAndWriteSingleReliableMessage) {
  FakeNetwork fake_network{};

  TestNeptun server{fake_network, kServerIp, kConnectionManagerConfig};
  TestNeptun client{fake_network, kClientIp, kConnectionManagerConfig};
  connect(server, client, fake_network);

  client.send_reliable_to(kServerIp, [](byte_span buffer) {
    IoBuffer io{buffer};
    auto count = io.write_string("this is test string", 0);
    return buffer.first(count);
  });
  client.tick(kNow, unexpected_reliable_msgs);

  usize msg_count = 0;
  server.tick(kNow, [&msg_count](byte_span payload) {
    IoBuffer io{payload};
    ASSERT_EQ(io.read_string(0), "this is test string");
    msg_count++;
  });
  ASSERT_EQ(msg_count, 1);
}

TEST(NeptunTest, ReadAndWriteSingleReliableMessage_DropPackets) {
  FakeNetwork fake_network{};
  TestNeptun server{fake_network, kServerIp, kConnectionManagerConfig};
  TestNeptun client{fake_network, kClientIp, kConnectionManagerConfig};
  connect(server, client, fake_network);

  client.send_reliable_to(kServerIp, [](byte_span buffer) {
    IoBuffer io{buffer};
    auto count = io.write_string("this is test string", 0);
    return buffer.first(count);
  });
  fake_network.drop_packets(true);
  client.tick(kNow, unexpected_reliable_msgs);

  usize reliable_msg_count = 0;
  server.tick(kNow, [&reliable_msg_count](byte_span payload) {
    reliable_msg_count++;
  });
  // Server never received any packets.
  ASSERT_EQ(reliable_msg_count, 0);

  fake_network.drop_packets(false);
  // We don't expect any reliable messages to be sent anymore because the client doesn't know
  // that the packets are dropped.
  client.tick(kNow, unexpected_reliable_msgs);
  reliable_msg_count = 0;
  server.tick(kNow, [&reliable_msg_count](byte_span payload) {
    reliable_msg_count++;
  });
  ASSERT_EQ(reliable_msg_count, 0);

  // Server responds to client with acks, but no reliable messages.
  // The acks tell the client that previously sent reliable messages haven't been delivered.
  server.tick(kNow, unexpected_reliable_msgs);

  reliable_msg_count = 0;
  client.tick(kNow, [&reliable_msg_count](byte_span payload) {
    reliable_msg_count++;
  });
  ASSERT_EQ(reliable_msg_count, 0);

  // The client resends the reliable message because it was dropped.
  client.tick(kNow, unexpected_reliable_msgs);

  reliable_msg_count = 0;
  server.tick(kNow, [&reliable_msg_count](byte_span payload) {
    IoBuffer io{payload};
    ASSERT_EQ(io.read_string(0), "this is test string");
    reliable_msg_count++;
  });
  ASSERT_EQ(reliable_msg_count, 1);
}

TEST(NeptunTest, ReadAndWriteMultipleReliableMessage) {
  FakeNetwork fake_network{};
  TestNeptun server{fake_network, kServerIp, kConnectionManagerConfig};
  TestNeptun client{fake_network, kClientIp, kConnectionManagerConfig};
  connect(server, client, fake_network);

  constexpr usize kNumMessages = 10;

  for (usize i = 0; i < kNumMessages; i++) {
    client.send_reliable_to(kServerIp, [i](byte_span buffer) {
      IoBuffer io{buffer};
      auto count = io.write_string("this is test string " + std::to_string(i), 0);
      return buffer.first(count);
    });
  }
  client.tick(kNow, unexpected_reliable_msgs);

  usize msg_count = 0;
  server.tick(kNow, [&msg_count](byte_span payload) {
    IoBuffer io{payload};
    ASSERT_EQ(io.read_string(0), "this is test string " + std::to_string(msg_count));
    msg_count++;
  });
  ASSERT_EQ(msg_count, 10);
}

TEST(NeptunTest, Timeouts) {
  constexpr u32 kPacketTimeoutSeconds = 5;

  FakeNetwork fake_network{};
  TestNeptun server{fake_network, kServerIp, kConnectionManagerConfig};
  TestNeptun client{fake_network, kClientIp, kConnectionManagerConfig, kPacketTimeoutSeconds};
  connect(server, client, fake_network);

  client.send_reliable_to(kServerIp, [](byte_span buffer) {
    IoBuffer io{buffer};
    auto count = io.write_string("this is test string", 0);
    return buffer.first(count);
  });
  fake_network.drop_packets(true);
  client.tick(kNow, unexpected_reliable_msgs);
  server.tick(kNow, unexpected_reliable_msgs);

  fake_network.drop_packets(false);
  client.tick(kNow + seconds(6), unexpected_reliable_msgs);
  usize msg_count = 0;
  server.tick(kNow, [&msg_count](byte_span payload) {
    IoBuffer io{payload};
    ASSERT_EQ(io.read_string(0), "this is test string");
    msg_count++;
  });
  ASSERT_EQ(msg_count, 1);
}

TEST(NeptunTest, ReliableMessageAfterDroppingMultiplePackets) {
  FakeNetwork fake_network{};
  TestNeptun server{fake_network, kServerIp, kConnectionManagerConfig};
  TestNeptun client{fake_network, kClientIp, kConnectionManagerConfig};
  connect(server, client, fake_network);

  auto send_to_server = [&client](usize sequence_number) {
    client.send_reliable_to(kServerIp, [sequence_number](byte_span buffer) {
      IoBuffer io{buffer};
      auto count = io.write_string("this is test string " + std::to_string(sequence_number), 0);
      return buffer.first(count);
    });
  };

  // Drop first 5 packets.
  fake_network.drop_packets(true);
  for (usize sequence_number = 0; sequence_number < 5; sequence_number++) {
    send_to_server(sequence_number);
    client.tick(kNow, unexpected_reliable_msgs);
  }

  // Add one more reliable message.
  // The packets are received on the server-side.
  fake_network.drop_packets(false);
  send_to_server(5);
  client.tick(kNow, unexpected_reliable_msgs);
  // Server doesn't receive any reliable messages yet, because the last one that succeeded
  // is dropped due to not being sent in order.
  server.tick(kNow, unexpected_reliable_msgs);

  // Client receives acks that the first 5 packets are dropped.
  // It sends all messages to the server again.
  client.tick(kNow, unexpected_reliable_msgs);

  usize msg_count = 0;
  server.tick(kNow, [&msg_count](byte_span buffer) {
    IoBuffer io{buffer};
    ASSERT_EQ(io.read_string(0), "this is test string " + std::to_string(msg_count));
    msg_count++;
  });
  ASSERT_EQ(msg_count, 6);
}

TEST(NeptunTest, UnreliableMessage) {
  FakeNetwork fake_network{};
  TestNeptun server{fake_network, kServerIp, kConnectionManagerConfig};
  TestNeptun client{fake_network, kClientIp, kConnectionManagerConfig};
  connect(server, client, fake_network);

  client.send_unreliable_to(kServerIp, [](byte_span buffer) {
    IoBuffer io{buffer};
    auto count = io.write_string("unreliable value", 0);
    return buffer.first(count);
  });

  client.tick(kNow);

  usize msg_count = 0;
  server.tick(kNow, unexpected_reliable_msgs, [&msg_count](byte_span buffer) {
    IoBuffer io{buffer};
    ASSERT_EQ(io.read_string(0), "unreliable value");
    msg_count++;
  });
  ASSERT_EQ(msg_count, 1);
}

TEST(NeptunTest, IgnoresPacketsForUnrelatedProtocol) {
  FAIL();
}

TEST(NeptunTest, PacketChecksum) {
  // Packet bits may be flipped because UDP only provides 16-bit checksums.
  // Rollout custom checksum and drop corrupt packets.
  FAIL();
}