#pragma once

#include "../pixmap.h"

#include <array>
#include <condition_variable>
#include <functional>

struct Area {
  uint16_t begin, end;
  std::function<void(uint8_t button)> action;
};

template <typename Mod>
class DynamicModule {
 public:
  void operator()[[noreturn]]() {
    static_cast<Mod&>(*this).update();

    while (true) {
      static_cast<Mod&>(*this).trigger();
      static_cast<Mod&>(*this).update();

      for (auto& cond : _conds) {
        cond->notify_one();
      }
    }
  }

  void subscribe(std::condition_variable* cond) {
    _conds.push_back(cond);
  }

 private:
  std::vector<std::condition_variable*> _conds;  // TODO array?
};


/** TODO
 * can we avoid spawning a thread for static modules?
 */
template <typename Mod>
class StaticModule {
 public:
  // TODO: can we avoid having to call these functions for StaticModule?
  void operator()() {}
  void subscribe(std::condition_variable* cond) {}
};
