#pragma once

#include <array>

class mod_clock {
 public:
  mod_clock() {}
  ~mod_clock() {}

  void trigger();
  std::string update();

 private:
  char clock_str[35];
};
