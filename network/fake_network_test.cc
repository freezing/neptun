//
// Created by freezing on 12/03/2022.
//

#include <gtest/gtest.h>

#include "fake_network.h"
#include "testing_helpers.h"

using namespace freezing::network;
using namespace freezing::network::testing;

namespace {

const char *kMessage = "This is test message.";
const std::size_t kMessageSize = strlen(kMessage);
const std::span<std::uint8_t> payload((std::uint8_t *) (kMessage), kMessageSize);
const auto ip = IpAddress::from_ipv4("192.168.0.10", 1000);
const auto destination = IpAddress::from_ipv4("192.168.0.20", 1234);

};

TEST(FakeNetworkTest, SocketFileDescriptorIsUnique) {
  FakeNetwork network{};
  std::set<FileDescriptor> fds;
  for (int i = 0; i < 1000; i++) {
    fds.insert(network.udp_socket_ipv4());
  }
  ASSERT_EQ(fds.size(), 1000);
}

TEST(FakeNetworkTest, BindSocket) {
  FakeNetwork network{};
  auto ip_address = IpAddress::from_ipv4("192.168.0.1", 7890);
  FileDescriptor socket = network.udp_socket_ipv4();
  network.bind(socket, ip_address);
}

TEST(FakeNetworkTest, SendAndRead) {
  FakeNetwork network{};
  auto sender_socket = network.udp_socket_ipv4();
  network.bind(sender_socket, ip);

  auto receiver_socket = network.udp_socket_ipv4();
  network.bind(receiver_socket, destination);

  std::size_t sent_count = network.send_to(sender_socket, destination, payload);
  ASSERT_EQ(sent_count, kMessageSize);

  std::vector<std::uint8_t> buffer;
  buffer.resize(1500);
  auto data = network.read_from_socket(receiver_socket, std::span(buffer));
  ASSERT_EQ(span_to_string(data), "This is test message.");
}

TEST(FakeNetworkTest, SendAndReadTooLargePacket) {
  const auto kMtu = 7;

  FakeNetwork network{kMtu};
  auto sender_socket = network.udp_socket_ipv4();
  network.bind(sender_socket, ip);

  auto receiver_socket = network.udp_socket_ipv4();
  network.bind(receiver_socket, destination);

  std::size_t sent_count = network.send_to(sender_socket, destination, payload);
  ASSERT_EQ(sent_count, kMessageSize);

  std::vector<std::uint8_t> buffer;
  buffer.resize(1500);
  auto data = network.read_from_socket(receiver_socket, std::span(buffer));
  ASSERT_EQ(span_to_string(data), "This is");
}