#pragma once

#include <cppcoro/generator.hpp>

#include <array>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <vector>

#include "../types.h"


template <typename Mod>
class DynamicModule {
 public:
  void operator() [[noreturn]] () {
    update();

    while (true) {
      static_cast<Mod&>(*this).trigger();
      update();

      for (auto& queue : _queues) {
        queue();
      }
    }
  }

  void subscribe(std::function<void()>&& queue) {
    _queues.push_back(std::forward<std::function<void()>>(queue));
  }


  cppcoro::generator<const segment_t&> get() const {
    std::unique_lock lock{_mutex};
    for (const auto& seg : static_cast<const Mod&>(*this)._segments) {
      co_yield seg;
    }
  }

 private:
  void update() {
    std::unique_lock lock{_mutex};
    static_cast<Mod&>(*this).update();
  }

  std::vector<std::function<void()>> _queues;
  mutable std::mutex _mutex;
};


template <typename Mod>
class StaticModule {
 public:
  // TODO: can we avoid having to call these functions for StaticModule?
  void operator()() {}
  void subscribe(std::function<void()>&&) {}
  cppcoro::generator<const segment_t&> get() const {
    for (const auto& seg : static_cast<const Mod&>(*this)._segments) {
      co_yield seg;
    }
  }
};


template <typename Mod, typename = void>
class ModuleContainer {
 public:
  template <typename... Args>
  ModuleContainer(Args&&... args)
      : _module(std::forward<Args>(args)...), _thread(std::ref(_module)) {}

  ~ModuleContainer() { _thread.join(); }

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
  template <typename... Args>
  ModuleContainer(Args&&... args) : _module(std::forward<Args>(args)...) {}

  Mod& operator*() { return _module; }

 private:
  Mod _module;
};
