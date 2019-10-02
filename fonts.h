#include "config.h"
#include "DisplayManager.h"

#include <algorithm>
#include <cstdint>
#include <xcb/xcb.h>
#include <X11/Xft/Xft.h>

struct font_t {
  font_t() = default;
  font_t(const char* pattern, int offset, xcb_connection_t *c, int scr_nbr);
  ~font_t() { DisplayManager::Instance()->xft_font_close(xft_ft); }

  bool font_has_glyph(const uint16_t c) {
    return DisplayManager::Instance()->xft_char_exists(xft_ft, (FcChar32) c);
  }

  XftFont *xft_ft { nullptr };
  int descent { 0 };
  int height { 0 };
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
