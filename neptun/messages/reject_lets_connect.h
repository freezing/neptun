//
// Created by freezing on 27/03/2022.
//

#ifndef NEPTUN_NEPTUN_MESSAGES_REJECT_LETS_CONNECT_H
#define NEPTUN_NEPTUN_MESSAGES_REJECT_LETS_CONNECT_H

#include "common/types.h"
#include "network/io_buffer.h"

namespace freezing::network {

// TODO: This should include max allowed packet rate and max allowed packet length
class RejectLetsConnect {
public:
  static constexpr u8 kId = 1;
  static constexpr usize kSerializedSize = 0;

  static byte_span write(byte_span buffer) {
    return buffer.first(0);
  }
};

}

#endif //NEPTUN_NEPTUN_MESSAGES_LETS_CONNECT_H
