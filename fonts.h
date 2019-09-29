#include "config.h"
#include "DisplayManager.h"

#include <algorithm>
#include <cstdint>
#include <xcb/xcb.h>
#include <X11/Xft/Xft.h>

struct font_t {
  font_t() = default;
  font_t(const char* pattern, int offset, xcb_connection_t *c, int scr_nbr);
  bool font_has_glyph(const uint16_t c);

  xcb_font_t ptr { 0 };
  xcb_charinfo_t *width_lut { nullptr };

  XftFont *xft_ft { nullptr };

  int ascent { 0 };

  int descent { 0 }, height { 0 }, width { 0 };
  uint16_t char_max { 0 };
  uint16_t char_min { 0 };
  int offset { 0 };
};


struct Fonts {
  void init(xcb_connection_t *c, int scr_nbr);
  font_t& operator[](size_t index) { return _fonts[index]; }
  font_t *current() {
    return _index >= 0 && _index < FONTS.size() ? &_fonts[_index] : nullptr;
  }
  font_t *select_drawable_font(const uint16_t c);

  std::array<font_t, FONTS.size()> _fonts;
  int _index;
};
