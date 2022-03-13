//
// Created by freezing on 12/03/2022.
//

#include <gtest/gtest.h>

#include "fake_network.h"
#include "udp_socket.h"
#include "testing_helpers.h"

using namespace freezing::network;
using namespace freezing::network::testing;

namespace {

const char *kMessage = "This is test message.";
const std::size_t kMessageSize = strlen(kMessage);
const std::span<std::uint8_t> payload((std::uint8_t *) (kMessage), kMessageSize);
const auto server_ip = IpAddress::from_ipv4("192.168.0.10", 1000);
const auto client_ip = IpAddress::from_ipv4("192.168.0.20", 1234);

}

TEST(UdpSocket, SendAndReceivePacket) {
  IoBuffer<1600> io_buffer;
  FakeNetwork network{};
  auto udp_server = UdpSocket<FakeNetwork>::bind(server_ip, network);
  auto udp_client = UdpSocket<FakeNetwork>::bind(client_ip, network);
  ASSERT_EQ(udp_server.read(io_buffer).size(), 0);
  ASSERT_EQ(udp_client.read(io_buffer).size(), 0);

  auto sent_count = udp_server.send_to(client_ip, payload);
  ASSERT_EQ(sent_count, kMessageSize);

  auto read_data = udp_client.read(io_buffer);
  ASSERT_EQ(span_to_string(read_data), "This is test message.");
}
