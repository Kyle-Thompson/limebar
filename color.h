#pragma once

#include <cstdint>
#include <cstring>

struct rgba_t {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;

  rgba_t() = default;
  rgba_t(uint32_t v) { set(v); }
  rgba_t(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_) : r(r_), g(g_), b(b_), a(a_) {}
  void set(uint32_t v) { memcpy(this, &v, sizeof(v)); }
  uint32_t* val() { return reinterpret_cast<uint32_t*>(this); }

  static rgba_t parse(const char *str);
};
