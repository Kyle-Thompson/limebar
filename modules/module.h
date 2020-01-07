#pragma once

#include "../pixmap.h"

#include <array>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>

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

  void get(ModulePixmap* px) const {
    // TODO: use reader/writer lock
    std::unique_lock lock{_mutex};
    static_cast<const Mod&>(*this).extract(px);
  }

 private:
  void update() {
    std::unique_lock lock{_mutex};
    static_cast<Mod&>(*this).update();
  }

  std::vector<std::condition_variable*> _conds;  // TODO array?
  mutable std::mutex _mutex;
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
  void get(ModulePixmap* px) const { static_cast<const Mod&>(*this).extract(px); }
};


// TODO: Specialize on StaticModule to not spawn a thread
template <typename Mod, typename = void>
class ModuleContainer {
 public:
  template <typename ...Args>
  ModuleContainer(Args&& ...args)
    : _module(std::forward<Args>(args)...)
    , _thread(std::ref(_module))
  {}

  ~ModuleContainer() {
    _thread.join();
  }

  Mod& operator*() { return _module; }

 private:
  Mod _module;
  std::thread _thread;
};


template <typename T>
using IsStaticModule = std::enable_if_t<std::is_base_of_v<StaticModule<T>, T>>;

template <typename Mod>
struct ModuleContainer<Mod, IsStaticModule<Mod>> {
 public:
  template <typename ...Args>
  ModuleContainer(Args&& ...args)
    : _module(std::forward<Args>(args)...)
  {}

  Mod& operator*() { return _module; }

 private:
  Mod _module;
};
