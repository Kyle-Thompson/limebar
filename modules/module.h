#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>

static std::condition_variable condvar;

struct Area {
  uint16_t begin, end;
  std::function<void(uint8_t button)> action;
};

template <typename Mod>
class Module {
 public:
  Module() = default;
  ~Module() = default;

  std::string get() {
    std::lock_guard<std::mutex> g(_mutex);
    return _str;
  }

  void operator()[[noreturn]] () {
    update();
    while (true) {
      _mod.trigger();
      update();
    }
  }

 private:
  void update() {
    std::lock_guard<std::mutex> g(_mutex);
    _str = _mod.update();
    condvar.notify_one();
  }

  Mod _mod;
  std::mutex _mutex;
  std::string _str;
  std::array<Area, Mod::MAX_AREAS> _areas;
};
