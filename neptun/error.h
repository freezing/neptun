//
// Created by freezing on 19/03/2022.
//

#ifndef NEPTUN_NEPTUN_ERROR_H
#define NEPTUN_NEPTUN_ERROR_H

#include <variant>

namespace freezing::network {

enum class NeptunError {
  MALFORMED_PACKET = 0,
  PEER_NOT_RESPONDING = 1,
  LETS_CONNECT_REJECTED = 2,
};

}

#endif //NEPTUN_NEPTUN_ERROR_H
