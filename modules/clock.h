#pragma once

#include "module.h"

#include <array>


class mod_clock : public module {
 public:
  mod_clock() {}
  ~mod_clock() {}

 private:
  void trigger();

  void update();

  char clock_str[35];
  static constexpr std::array<const char*, 12> months {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", };
};
