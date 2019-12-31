#pragma once

#include "module.h"
#include <array>

// TODO: replace with std::string when that becomes standardized
class mod_fill : public StaticModule<mod_fill> {
 public:
  explicit mod_fill(const char* str) : _str(str) {}

  template <typename DS>
  void extract(ModulePixmap<DS>* px) const {
    px->write(_str);
  }
  
 private:
  const char* _str;
};
