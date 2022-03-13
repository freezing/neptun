//
// Created by freezing on 12/03/2022.
//

#include <gtest/gtest.h>

#include "message.h"

using namespace freezing::network;

TEST(MessageTest, Ping) {
  std::vector<std::uint8_t> buffer(1600);
  auto payload = Ping::write(buffer, 0x01020304, "test note");
  auto ping = Ping(payload);
  ASSERT_EQ(ping.timestamp(), 0x01020304);
  ASSERT_EQ(ping.note(), "test note");
}

