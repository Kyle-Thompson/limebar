#pragma once

#include <array>

#include "module.h"

// TODO: replace with std::string when that becomes standardized
class mod_fill : public StaticModule<mod_fill> {
 public:
  explicit mod_fill(const char* str)
      : _segments({segment_t{.segments{{.str = str, .color = NORMAL_COLOR}}}}) {
  }

  cppcoro::generator<segment_t> extract() const {
    for (auto seg : _segments) {
      co_yield seg;
    }
  }

 private:
  std::array<segment_t, 1> _segments;
};
