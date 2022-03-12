//
// Created by freezing on 12/03/2022.
//

#include <gtest/gtest.h>

#include "message.h"

using namespace freezing::network;

TEST(MessageTest, Ping) {
  IoBuffer io_buffer{};
  Ping<>::write(io_buffer, 0x01020304, "test note");
  auto ping = Ping(io_buffer);
  ASSERT_EQ(ping.timestamp(), 0x01020304);
  ASSERT_EQ(ping.note(), "test note");
}

