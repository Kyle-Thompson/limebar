#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

struct rgba_t {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
  std::array<char, 8> str { "#000000" };

  rgba_t() = default;
  rgba_t(uint32_t v) { set(v); parse_str(); }
  rgba_t(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_)
    : r(r_)
    , g(g_)
    , b(b_)
    , a(a_)
  {
    parse_str();
  }

  void set(uint32_t v) { memcpy(this, &v, sizeof(v)); }
  uint32_t* val() { return reinterpret_cast<uint32_t*>(this); }
  char *get_str() { return str.data(); }

  void parse_str() {
    // TODO: why bgr instead of rgb?
    snprintf(str.data(), 8, "#%X%X%X", b, g, r);
  }

  static rgba_t parse(const char *str);
};
