#pragma once

#include "module.h"

class mod_clock : public DynamicModule<mod_clock> {
  friend class DynamicModule<mod_clock>;

  void trigger();
  void update();

  std::array<segment_t, 1> _segments;
};
