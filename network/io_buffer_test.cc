//
// Created by freezing on 12/03/2022.
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "message.h"

using namespace freezing::network;

// TODO: Improve
std::vector<std::uint8_t> to_vec(std::span<std::uint8_t> data) {
  return std::vector<std::uint8_t>(data.begin(), data.end());
}

TEST(IoBufferTest, ReadWriteU8) {
  IoBuffer io_buffer{};
  io_buffer.write_u8(0x17);
  io_buffer.flip();
  ASSERT_EQ(io_buffer.read_u8(), 0x17);
}

TEST(IoBufferTest, ReadWriteU16) {
  IoBuffer io_buffer{};
  io_buffer.write_u16(0xa1b2);
  io_buffer.flip();
  ASSERT_EQ(io_buffer.read_u16(), 0xa1b2);
}

TEST(IoBufferTest, ReadWriteU32) {
  IoBuffer io_buffer{};
  io_buffer.write_u32(0x01020304);
  io_buffer.flip();
  ASSERT_EQ(io_buffer.read_u32(), 0x01020304);
}

TEST(IoBufferTest, ReadWriteString) {
  IoBuffer io_buffer{};
  io_buffer.write_string("this is test value");
  io_buffer.flip();
  ASSERT_EQ(io_buffer.read_string(), "this is test value");
}

TEST(IoBufferTest, NetworkEndianessU8) {
  IoBuffer io_buffer{};
  io_buffer.write_u8(0xa1);
  io_buffer.flip();
  auto actual = to_vec(io_buffer.at_pos().first(sizeof(std::uint8_t)));
  ASSERT_THAT(actual, testing::ElementsAre(0xa1));
}

TEST(IoBufferTest, NetworkEndianessU16) {
  IoBuffer io_buffer{};
  io_buffer.write_u16(0x0102);
  io_buffer.flip();
  auto actual = to_vec(io_buffer.at_pos().first(sizeof(std::uint16_t)));
  ASSERT_THAT(actual, testing::ElementsAre(0x01, 0x02));
}

TEST(IoBufferTest, NetworkEndianessU32) {
  IoBuffer io_buffer{};
  io_buffer.write_u32(0x01020304);
  io_buffer.flip();
  auto actual = to_vec(io_buffer.at_pos().first(sizeof(std::uint32_t)));
  ASSERT_THAT(actual, testing::ElementsAre(0x01, 0x02, 0x03, 0x04));
}