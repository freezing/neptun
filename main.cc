#include <iostream>
#include <cassert>
#include <vector>
#include <span>
#include <string>

#include "ring_buffer.h"

int main(int argc, char** argv) {
  std::uint8_t data[] = {10, 20, 30, 40, 50, 60};
  RingBuffer<std::uint8_t> buffer{64};
  buffer.write(data);
  std::string sep;
  for (const std::uint8_t t : buffer) {
    printf("%s%d", sep.c_str(), t);
    sep = " ";
  }
  std::cout << std::endl;
  return 0;
}