//
// Created by freezing on 12/03/2022.
//

#include <gtest/gtest.h>

#include "ip_address.h"

using namespace freezing::network;

TEST(IpAddressTest, Encode) {
  auto host = detail::ip_string_to_host("192.168.0.1");
  struct sockaddr_in address{};
  address.sin_addr.s_addr = htonl(host);
  std::string actual(inet_ntoa(address.sin_addr));
  ASSERT_EQ(actual, "192.168.0.1");
}

TEST(IpAddressTest, EncodeAndDecode) {
  auto host = detail::ip_string_to_host("192.168.0.1");
  auto string = detail::ip_host_to_string(host);
  ASSERT_EQ(string, "192.168.0.1");
}

TEST(IpAddressTest, IPv4ToString) {
  auto ip = IpAddress::from_ipv4("192.168.0.1", 1234);
  ASSERT_EQ(ip.to_string(), "192.168.0.1:1234");
}