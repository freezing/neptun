#ifndef NEPTUN__RING_BUFFER_H_
#define NEPTUN__RING_BUFFER_H_

#include <span>

template<typename T>
class RingBuffer {
public:
  RingBuffer(int capacity) : m_capacity{capacity} {
    m_buffer = new T[capacity];
    m_begin = m_buffer;
    m_end = m_buffer;
  }

  size_t length() const {
    if (m_end >= m_begin) {
      return m_end - m_begin;
    }
    return m_begin - m_end;
  }

  size_t remaining_capacity() const {
    return m_capacity - length();
  }

  
  void write(std::span<T> span) {
    assert(span.size() <= remaining_capacity());
    for (const T& t : span) {
      *m_end = t;
      advance(&m_end);
    }
  }

  void reset() {
    m_begin = m_end = m_buffer;
  }

  void set_window(int start, int end) {
    m_begin = m_buffer + start;
    m_end = m_buffer + end;
  }

  T* begin() const {
    return m_begin;
  }

  T* end() const {
    return m_end;
  }

  ~RingBuffer() {
    delete m_buffer;
  }

private:
  T* m_begin;
  T* m_end;
  T* m_buffer;
  int m_capacity;

  void advance(T** ptr) {
    if ((*ptr) == m_buffer + m_capacity) {
      *ptr = m_buffer;
    } else {
      (*ptr)++;
    }
  }
};

#endif