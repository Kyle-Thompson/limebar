#pragma once

#include <bits/stdint-uintn.h>

#include <array>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>

template <typename T, size_t N = 10>
class StaticWorkQueue {
 public:
  StaticWorkQueue() = default;

  void push(T t);
  void push(T&& t);
  T&& pop();

 private:
  /* alignas(T) std::byte _arr[sizeof(T) * N]; */
  alignas(T) std::array<std::byte, sizeof(T) * N> _arr;
  size_t _head{0}, _size{0};
  std::condition_variable _write_condvar;
  std::condition_variable _read_condvar;
  std::mutex _write_mu;
  std::mutex _read_mu;
};


template <typename T, size_t N>
void
StaticWorkQueue<T, N>::push(T t) {
  std::unique_lock lock{_write_mu};
  if (_size == N) {
    _write_condvar.wait(lock, [&] { return _size != N; });
  }

  new (&_arr[(_head + _size) * sizeof(T) % N]) T(std::move(t));
  ++_size;
  _read_condvar.notify_one();
}


template <typename T, size_t N>
void
StaticWorkQueue<T, N>::push(T&& t) {
  std::unique_lock lock{_write_mu};
  if (_size == N) {
    _write_condvar.wait(lock, [&] { return _size != N; });
  }

  new (&_arr[(_head + _size) * sizeof(T) % N]) T(std::forward<T>(t));
  ++_size;
  _read_condvar.notify_one();
}


template <typename T, size_t N>
T&&
StaticWorkQueue<T, N>::pop() {
  std::unique_lock lock{_read_mu};
  if (!_size) {
    _read_condvar.wait(lock, [&] { return _size > 0; });
  }

  T&& temp = std::move(static_cast<T>(_arr[_head]));
  _head = _head + sizeof(T) % N;
  --_size;

  _write_condvar.notify_one();
  return std::move(temp);
}


template <typename T>
std::function<void()>
register_queue(StaticWorkQueue<T>* queue, T&& elem) {
  return [queue, e = std::forward<T>(elem)] { queue->push(e); };
}
