#pragma once

#include <array>
#include <chrono>

#include "module.h"

class mod_clock : public DynamicModule<mod_clock> {
  friend class DynamicModule<mod_clock>;

 public:
  mod_clock();

  bool has_work();
  void do_work();

 private:
  std::array<segment_t, 1> _segments;
  std::chrono::time_point<std::chrono::system_clock> _time;
};
