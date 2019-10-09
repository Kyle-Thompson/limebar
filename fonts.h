#pragma once

#include "config.h"
#include "x.h"

#include <algorithm>
#include <cstdint>
#include <xcb/xcb.h>
#include <X11/Xft/Xft.h>

struct font_t {
  font_t() = default;
  font_t(const char* pattern, int offset);
  ~font_t() { X::Instance()->xft_font_close(xft_ft); }

  bool font_has_glyph(const uint16_t c) {
    return X::Instance()->xft_char_exists(xft_ft, (FcChar32) c);
  }

  XftFont *xft_ft { nullptr };
  int descent { 0 };
  int height { 0 };
  int offset { 0 };
};

struct Fonts {
  Fonts();
  void init();
  font_t& operator[](size_t index) { return _fonts[index]; }
  font_t& drawable_font(const uint16_t c);

  std::array<font_t, FONTS.size()> _fonts;
};
