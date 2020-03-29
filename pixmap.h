#pragma once

#include <X11/Xft/Xft.h>
#include <bits/stdint-uintn.h>  // uint16_t
#include <xcb/xproto.h>         // xcb_drawable_t

#include <algorithm>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include "bar_color.h"
#include "config.h"
#include "font.h"
#include "types.h"

using ucs2 = std::vector<uint16_t>;
using ucs2_and_width = std::pair<ucs2, size_t>;

struct Util {
  // TODO: find a better home for this
  static ucs2 utf8_to_ucs2(const std::string& text) {
    ucs2 str;

    for (uint8_t* utf = (uint8_t*)text.c_str();
         utf != (uint8_t*)&*text.end();) {
      uint16_t ucs = 0;
      // ASCII
      if (utf[0] < 0x80) {
        ucs = utf[0];
        utf += 1;
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
    }

    return str;
  }
};

// TODO: create a wrapper class that only allows appending and not clearing to
// limit access to what modules can do when getting a ModulePixmap
class ModulePixmap {
 public:
  ModulePixmap(xcb_drawable_t drawable, BarColors* colors, Fonts* fonts,
               uint16_t width, uint16_t height)
      : _used(0)
      , _width(width)
      , _height(height)
      , _ds(DS::Instance())
      , _colors(colors)
      , _fonts(fonts)
      , _pixmap_id(_ds.generate_id())
      , _xft_draw(_ds.xft_draw_create(_pixmap_id)) {
    _ds.create_pixmap(_pixmap_id, drawable, width, height);
    clear();
  }

  ~ModulePixmap() { DS::Instance().free_pixmap(_pixmap_id); }

  ModulePixmap(const ModulePixmap&) = delete;
  ModulePixmap(ModulePixmap&&) = delete;
  ModulePixmap& operator=(const ModulePixmap&) = delete;
  ModulePixmap& operator=(ModulePixmap&&) = delete;

  [[nodiscard]] uint16_t size() const { return _used; }
  [[nodiscard]] xcb_pixmap_t pixmap() const { return _pixmap_id; }

  void clear() {
    _used = 0;
    _areas = std::vector<area_t>();
    _ds.clear_rect(_pixmap_id, _width, _height);
  }

  void append(const ModulePixmap& rhs) {
    _ds.copy_area(rhs.pixmap(), _pixmap_id, 0, _used, rhs.size(), _height);
    _used += rhs._used;
  }

  void click(size_t x, uint8_t button) {
    for (auto& area : _areas) {
      if (x >= area.begin && x <= area.end) {
        area.action(button);
        break;
      }
    }
  }

  void write(const segment_t& seg) {
    using FontType = typename Fonts::Font;
    using StringContainer = std::tuple<ucs2, FontType*, size_t>;

    std::vector<StringContainer> ucs2_vec;
    ucs2_vec.reserve(seg.segments.size());
    for (const auto& s : seg.segments) {
      ucs2 str = Util::utf8_to_ucs2(s.str);
      FontType* font = _fonts->drawable_font(str[0]);
      ucs2_vec.emplace_back(std::move(str), font, font->string_size(str));
    }

    const size_t total_size =
        std::accumulate(ucs2_vec.begin(), ucs2_vec.end(), 0,
                        [](size_t cur, const StringContainer& p) {
                          return cur + std::get<2>(p);
                        });

    if (_used + total_size <= _width) {
      for (size_t i = 0; i < ucs2_vec.size(); ++i) {
        auto [str, font, size] = ucs2_vec[i];
        FontColor* color = seg.segments[i].color == NORMAL_COLOR
                               ? &_colors->foreground
                               : &_colors->fg_accent;

        DS::draw_ucs2_string(_xft_draw, font, color, str, _height, _used);
        if (seg.action) {
          uint16_t end = _used + size;
          _areas.push_back({.begin = _used, .end = end, .action = *seg.action});
          _used = end;
        } else {
          _used += size;
        }
      }
    }
  }

 private:
  uint16_t _used;
  uint16_t _width, _height;
  DS& _ds;
  BarColors* _colors;
  Fonts* _fonts;
  xcb_pixmap_t _pixmap_id;
  XftDraw* _xft_draw;
  std::vector<area_t> _areas;
};
