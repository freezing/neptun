//
// Created by freezing on 14/03/2022.
//

#ifndef NEPTUN_COMMON_FLIP_BUFFER_H
#define NEPTUN_COMMON_FLIP_BUFFER_H

#include <cassert>
#include <vector>
#include <span>


namespace freezing {

template<typename T>
class FlipBuffer {
public:
  explicit FlipBuffer(usize capacity) : m_buffer(capacity) {}

  std::span<T> remaining() {
    return std::span(m_buffer.begin() + m_end, m_buffer.end());
  }

  std::span<T> data() {
    return std::span(m_buffer.begin() + m_begin, m_buffer.begin() + m_end);
  }

  void flip() {
    usize new_end = m_end - m_begin;
    // TODO: Use memcpy for efficiency.
    for (usize i = 0; i < new_end; i++) {
      m_buffer[i] = m_buffer[m_begin + i];
    }
    m_begin = 0;
    m_end = new_end;
  }

  typename std::vector<T>::iterator begin() {
    return m_buffer.begin();
  }

  usize begin_index() const {
    return m_begin;
  }

  usize end_index() const {
    return m_end;
  }

  void consume(usize count) {
    m_begin += count;
  }

  void advance(usize count) {
    assert(m_end + count < m_buffer.capacity());
    m_end += count;
  }

private:
  usize m_begin{0};
  usize m_end{0};
  std::vector<u8> m_buffer;
};

}

#endif //NEPTUN_COMMON_FLIP_BUFFER_H
