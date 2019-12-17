#pragma once

#include "module.h"
#include <array>

// TODO: replace with std::string when that becomes standardized
class mod_fill : public StaticModule<mod_fill> {
 public:
  explicit mod_fill(const char* str) : _str(str) {}

  void get(ModulePixmap* px) {
    px->write(_str);
  }
  
 private:
  const char* _str;
};
