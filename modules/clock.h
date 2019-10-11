#pragma once

#include <array>

class mod_clock {
 public:
  mod_clock() {}
  ~mod_clock() {}

  void trigger();
  std::string update();

  constexpr static size_t MAX_AREAS = 1;

 private:
  char clock_str[35];
};
