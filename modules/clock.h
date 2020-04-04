#pragma once

#include "module.h"

class mod_clock : public DynamicModule<mod_clock> {
  friend class DynamicModule<mod_clock>;

  cppcoro::generator<segment_t> extract() const;
  void trigger();
  void update();

  std::array<char, 6> current_time;
  std::array<char, 8> current_date;
};
