#pragma once

#include <bits/stdint-uintn.h>

#include <array>
#include <atomic>
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
  alignas(T) std::array<std::byte, sizeof(T) * N> _arr;
  size_t _head{0};
  std::atomic_size_t _size{0};
  std::condition_variable _write_condvar;
  std::condition_variable _read_condvar;
  std::mutex _write_mu;
  std::mutex _read_mu;
  std::mutex _main_mu;  // TODO: can this be replaced with a non-global lock
};


template <typename T, size_t N>
void
StaticWorkQueue<T, N>::push(T t) {
  std::unique_lock write_lock{_write_mu};
  if (_size == N) {
    _write_condvar.wait(write_lock, [&] { return _size != N; });
  }
  write_lock.unlock();

  std::unique_lock main_lock{_main_mu};
  new (&_arr[(_head + _size) * sizeof(T) % N]) T(std::move(t));
  ++_size;
  _read_condvar.notify_one();
}


template <typename T, size_t N>
void
StaticWorkQueue<T, N>::push(T&& t) {
  std::unique_lock write_lock{_write_mu};
  if (_size == N) {
    _write_condvar.wait(write_lock, [&] { return _size != N; });
  }
  write_lock.unlock();

  std::unique_lock main_lock{_main_mu};
  new (&_arr[(_head + _size) * sizeof(T) % N]) T(std::forward<T>(t));
  ++_size;
  _read_condvar.notify_one();
}


template <typename T, size_t N>
T&&
StaticWorkQueue<T, N>::pop() {
  std::unique_lock read_lock{_read_mu};
  if (!_size) {
    _read_condvar.wait(read_lock, [&] { return _size > 0; });
  }
  read_lock.unlock();

  std::unique_lock main_lock{_main_mu};
  T&& temp = std::move(static_cast<T>(_arr[(_head + _size) * sizeof(T) % N]));
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
