#pragma once

#include "module.h"

#include <array>
#include <utility>

class mod_clock : public Module<mod_clock> {
 public:
  mod_clock(const BarWindow& win) : Module(win) {}

  void trigger();
  void update();

  constexpr static size_t MAX_AREAS = 1;
};
