//
// Created by freezing on 14/03/2022.
//

#ifndef NEPTUN_NEPTUN_MESSAGES_SEGMENT_H
#define NEPTUN_NEPTUN_MESSAGES_SEGMENT_H

#include "common/types.h"
#include "network/io_buffer.h"

namespace freezing::network {

enum ManagerType {
  CONNECTION_MANAGER = 0,
  MOVE_MANAGER = 1,
  LATEST_STATE_MANAGER = 2,
  // TODO: Rename to STREAM -> MANAGER
  RELIABLE_STREAM = 3,
  UNRELIABLE_STREAM = 4,
};

class Segment {
public:
  static constexpr usize kManagerTypeOffset = 0;
  static constexpr usize kMessageCountOffset = kManagerTypeOffset + sizeof(u8);

  static constexpr usize kSerializedSize = sizeof(u8) + sizeof(u8);

  static byte_span write(byte_span buffer, u8 manager_type, u8 message_count) {
    auto io = IoBuffer(buffer);
    usize count = 0;
    count += io.write_u8(manager_type, kManagerTypeOffset);
    count += io.write_u8(message_count, kMessageCountOffset);
    return buffer.first(count);
  }

  explicit Segment(byte_span buffer) : m_buffer{buffer} {}

  u8 manager_type() const {
    return m_buffer.read_u8(kManagerTypeOffset);
  }

  u8 message_count() const {
    return m_buffer.read_u8(kMessageCountOffset);
  }

private:
  IoBuffer m_buffer;
};

}

#endif //NEPTUN_NEPTUN_MESSAGES_SEGMENT_H
