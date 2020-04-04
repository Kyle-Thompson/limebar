#pragma once

#include <array>

#include "module.h"

// TODO: replace with std::string when that becomes standardized
class mod_fill : public StaticModule<mod_fill> {
 public:
  explicit mod_fill(const char* str) : _str(str) {}

  cppcoro::generator<segment_t> extract() const {
    co_yield {.segments{{.str = _str, .color = NORMAL_COLOR}}};
  }

 private:
  const char* _str;
};
