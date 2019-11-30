#pragma once

#include "dynamic_module.h"

#include <array>
#include <utility>

class mod_clock : public DynamicModule<mod_clock> {
 public:
  mod_clock(const BarWindow& win) : DynamicModule(win) {}

  void trigger();
  void update();

  constexpr static size_t MAX_AREAS = 1;
};
