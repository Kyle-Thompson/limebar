#pragma once

#include "module.h"

class mod_clock : public DynamicModule<mod_clock> {
  friend class DynamicModule<mod_clock>;

  template <typename DS>
  void extract(ModulePixmap<DS>* px) const;
  void trigger();
  void update();

  std::array<char, 6> current_time;
  std::array<char, 8> current_day;
};

template <typename DS>
void mod_clock::extract(ModulePixmap<DS>* px) const {
  px->write(current_time.data(), true);
  px->write(current_day.data());
}
