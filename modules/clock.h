#pragma once

#include "module.h"

class mod_clock : public DynamicModule<mod_clock> {
 public:
  void get(ModulePixmap& px);

  constexpr static size_t MAX_AREAS = 1;

  friend class DynamicModule<mod_clock>;
 private:
  void trigger();
  void update();

  char current_time[6];
  char current_day[8];
};
