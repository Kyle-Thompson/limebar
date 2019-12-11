#pragma once

#include "module.h"

class mod_clock : public DynamicModule<mod_clock> {
  friend class DynamicModule<mod_clock>;

  void extract(ModulePixmap& px) const;
  void trigger();
  void update();

  char current_time[6];
  char current_day[8];
};
