//
// Created by freezing on 12/03/2022.
//

#ifndef NEPTUN__TESTING_HELPERS_H
#define NEPTUN__TESTING_HELPERS_H

#include <string>
#include <span>

static std::string span_to_string(std::span<std::uint8_t> data) {
  std::string s{};
  for (char c : data) {
    s += c;
  }
  return s;
}

#endif //NEPTUN__TESTING_HELPERS_H
