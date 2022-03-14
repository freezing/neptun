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
  std::vector<std::uint8_t> buffer(1600);
  auto io = IoBuffer(buffer);
  ASSERT_EQ(io.write_u8(0x17, 0), 1);
  ASSERT_EQ(io.read_u8(0), 0x17);
}

TEST(IoBufferTest, ReadWriteU16) {
  std::vector<std::uint8_t> buffer(1600);
  auto io = IoBuffer(buffer);
  ASSERT_EQ(io.write_u16(0xa1b2, 0), 2);
  ASSERT_EQ(io.read_u16(0), 0xa1b2);
}

TEST(IoBufferTest, ReadWriteU32) {
  std::vector<std::uint8_t> buffer(1600);
  auto io = IoBuffer(buffer);
  io.write_u32(0x01020304, 0);
  ASSERT_EQ(io.read_u32(0), 0x01020304);
}

TEST(IoBufferTest, ReadWriteString) {
  std::vector<std::uint8_t> buffer(1600);
  auto io = IoBuffer(buffer);
  ASSERT_EQ(io.write_string("this is test value", 0), sizeof(std::uint16_t) + strlen("this is test value"));
  ASSERT_EQ(io.read_string(0), "this is test value");
}

TEST(IoBufferTest, NetworkEndianessU8) {
  std::vector<std::uint8_t> buffer(1600);
  auto io = IoBuffer(buffer);
  io.write_u8(0xa1, 0);
  auto actual = to_vec(io.read_byte_array(0, sizeof(std::uint8_t)));
  ASSERT_THAT(actual, testing::ElementsAre(0xa1));
}

TEST(IoBufferTest, NetworkEndianessU16) {
  std::vector<std::uint8_t> buffer(1600);
  auto io = IoBuffer(buffer);
  ASSERT_EQ(io.write_u16(0x0102, 0), 2);
  auto actual = to_vec(io.read_byte_array(0, sizeof(std::uint16_t)));
  ASSERT_THAT(actual, testing::ElementsAre(0x01, 0x02));
}

TEST(IoBufferTest, NetworkEndianessU32) {
  std::vector<std::uint8_t> buffer(1600);
  auto io = IoBuffer(buffer);
  io.write_u32(0x01020304, 0);
  auto actual = to_vec(io.read_byte_array(0, sizeof(std::uint32_t)));
  ASSERT_THAT(actual, testing::ElementsAre(0x01, 0x02, 0x03, 0x04));
}