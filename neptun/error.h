//
// Created by freezing on 19/03/2022.
//

#ifndef NEPTUN_NEPTUN_ERROR_H
#define NEPTUN_NEPTUN_ERROR_H

#include <variant>

namespace freezing::network {

enum class NeptunError {
  MALFORMED_PACKET = 0
};

}

#endif //NEPTUN_NEPTUN_ERROR_H
