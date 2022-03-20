//
// Created by freezing on 15/03/2022.
//

#ifndef NEPTUN_NEPTUN_COMMON_H
#define NEPTUN_NEPTUN_COMMON_H

namespace freezing::network {

enum class PacketDeliveryStatus {
  ACK,
  DROP,
};

using PacketId = u32;
using AckSequenceNumber = u32;
using AckBitmask = u32;

}

#endif //NEPTUN_NEPTUN_COMMON_H
