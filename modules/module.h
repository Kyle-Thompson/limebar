#pragma once

#include <cppcoro/generator.hpp>
#include <functional>
#include <type_traits>

#include "../types.h"


template <typename Mod>
class DynamicModule {
 public:
  cppcoro::generator<const segment_t&> get() const {
    for (const auto& seg : static_cast<const Mod&>(*this)._segments) {
      co_yield seg;
    }
  }
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
