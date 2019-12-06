#pragma once

#include "../pixmap.h"

#include <array>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <shared_mutex>

struct Area {
  uint16_t begin, end;
  std::function<void(uint8_t button)> action;
};

template <typename Mod>
class DynamicModule {
 public:
  void operator()[[noreturn]]() {
    update();

    while (true) {
      static_cast<Mod&>(*this).trigger();
      update();

      for (auto& cond : _conds) {
        cond->notify_one();
      }
    }
  }

  void subscribe(std::condition_variable* cond) {
    _conds.push_back(cond);
  }

  void update() {
    /* std::unique_lock lock{_mutex}; */
    static_cast<Mod&>(*this).update();
  }

  void get(ModulePixmap& px) const {
    /* std::shared_lock lock{_mutex}; */
    static_cast<Mod&>(*this).get(px);
  }

 private:
  std::vector<std::condition_variable*> _conds;  // TODO array?
  mutable std::shared_mutex _mutex;
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
