#pragma once

#include "x.h"
#include "color.h"

#include <X11/Xft/Xft.h>
#include <algorithm>
#include <bits/stdint-uintn.h>  // uint16_t
#include <mutex>
#include <string>
#include <vector>
#include <xcb/xproto.h>  // xcb_drawable_t

using ucs2 = std::vector<uint16_t>;
using ucs2_and_width = std::pair<ucs2, size_t>;

struct Util {
  // TODO: find a better home for this
  static ucs2_and_width utf8_to_ucs2(const std::string& text) {
    size_t total_width = 0;
    ucs2 str;

    for (uint8_t *utf = (uint8_t *)text.c_str(); utf != (uint8_t *) &*text.end();) {
      uint16_t ucs = 0;
      // ASCII
      if (utf[0] < 0x80) {
        ucs = utf[0];
        utf  += 1;
      }
      // Two byte utf8 sequence
      else if ((utf[0] & 0xe0) == 0xc0) {
        ucs = (utf[0] & 0x1f) << 6 | (utf[1] & 0x3f);
        utf += 2;
      }
      // Three byte utf8 sequence
      else if ((utf[0] & 0xf0) == 0xe0) {
        ucs = (utf[0] & 0xf) << 12 | (utf[1] & 0x3f) << 6 | (utf[2] & 0x3f);
        utf += 3;
      }
      // Four byte utf8 sequence
      else if ((utf[0] & 0xf8) == 0xf0) {
        ucs = 0xfffd;
        utf += 4;
      }
      // Five byte utf8 sequence
      else if ((utf[0] & 0xfc) == 0xf8) {
        ucs = 0xfffd;
        utf += 5;
      }
      // Six byte utf8 sequence
      else if ((utf[0] & 0xfe) == 0xfc) {
        ucs = 0xfffd;
        utf += 6;
      }
      // Not a valid utf-8 sequence
      else {
        ucs = utf[0];
        utf += 1;
      }

      str.push_back(ucs);
      total_width += X::Instance().xft_char_width(ucs);
    }

    return std::make_pair(str, total_width);
  }
};

// TODO: create a wrapper class that only allows appending and not clearing to
// limit access to what modules can do when getting a ModulePixmap
template <typename DS>
class ModulePixmap {
 public:
  ModulePixmap(xcb_drawable_t drawable, BarColors<DS>* colors, uint16_t width,
               uint16_t height)
    : _used(0)
    , _width(width)
    , _height(height)
    , _x(X::Instance())
    , _colors(colors)
    , _pixmap_id(_x.generate_id())
    , _xft_draw(_x.xft_draw_create(_pixmap_id))
  {
    _x.create_pixmap(_pixmap_id, drawable, width, height);
    clear();
  }

  ~ModulePixmap() {
    X::Instance().free_pixmap(_pixmap_id);
  }

  ModulePixmap(const ModulePixmap&) = delete;
  ModulePixmap(ModulePixmap&&) = delete;
  ModulePixmap& operator=(const ModulePixmap&) = delete;
  ModulePixmap& operator=(ModulePixmap&&) = delete;

  [[nodiscard]] uint16_t     size()   const { return _used; }
  [[nodiscard]] xcb_pixmap_t pixmap() const { return _pixmap_id; }

  void clear() {
    _used = 0;
    _x.clear_rect(_pixmap_id, _width, _height);
  }

  void append(const ModulePixmap& rhs) {
    _x.copy_area(rhs.pixmap(), _pixmap_id, 0, _used, rhs.size(), _height);
    _used += rhs._used;
  }

  // TODO: overload with const char*
  void write(const std::string& str, bool accented = false) {
    ucs2_and_width parsed = Util::utf8_to_ucs2(str);

    // TODO: clamp to max instead of not writing anything
    if (_used + parsed.second <= _width) {
      if (accented) {
        _x.draw_ucs2_string(_xft_draw, &_colors->fg_accent, parsed.first,
                            _used);
      } else {
        _x.draw_ucs2_string(_xft_draw, &_colors->foreground, parsed.first,
                            _used);
      }
      _used += parsed.second;
    }
  }

 private:
  uint16_t        _used;
  uint16_t        _width, _height;
  X&              _x;
  BarColors<DS>*  _colors;
  xcb_pixmap_t    _pixmap_id;
  XftDraw*        _xft_draw;
  std::mutex      _mutex;
};
