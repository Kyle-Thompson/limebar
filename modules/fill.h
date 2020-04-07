#pragma once

#include <array>

#include "module.h"

class mod_fill : public StaticModule<mod_fill> {
  friend class StaticModule<mod_fill>;
 public:
  explicit mod_fill(const char* str)
      : _segments({segment_t{.segments{{.str = str, .color = NORMAL_COLOR}}}}) {
  }

 private:
  std::array<segment_t, 1> _segments;
};
