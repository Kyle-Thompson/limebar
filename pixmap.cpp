#include <range/v3/view/transform.hpp>
#include <range/v3/numeric/accumulate.hpp>

#include "pixmap.h"


ucs2 utf8_to_ucs2(const std::string& text) {
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


ModulePixmap::ModulePixmap(DS::pixmap_t pixmap, BarColors* colors, Fonts* fonts,
                           uint16_t width, uint16_t height)
      : _used(0)
      , _width(width)
      , _height(height)
      , _ds(DS::Instance())
      , _colors(colors)
      , _fonts(fonts)
      , _pixmap(std::move(pixmap))
      , _xft_draw(_pixmap.create_xft_draw()) {}


void
ModulePixmap::clear() {
  _used = 0;
  _areas = std::vector<area_t>();
  _pixmap.clear();
}


void
ModulePixmap::append(const ModulePixmap& rhs) {
  // TODO: figure out what the right type is to use for _used
  _pixmap.copy_from(rhs._pixmap, {0, 0}, {static_cast<int16_t>(_used), 0},
                    rhs.size(), _height);
  _used += rhs._used;
}


void
ModulePixmap::click(size_t x, uint8_t button) {
  for (auto& area : _areas) {
    if (x >= area.begin && x <= area.end) {
      area.action(button);
      break;
    }
  }
}


void
ModulePixmap::write(const segment_t& seg) {
  using FontType = typename Fonts::Font;
  using StringContainer = std::tuple<ucs2, FontType*, size_t>;

  const auto ucs2_vec = seg.segments
      | ranges::views::transform([this](const auto& s) -> StringContainer {
             ucs2 str = utf8_to_ucs2(s.str);
             FontType* font = _fonts->drawable_font(str[0]);
             return {str, font, font->string_size(str)};
         });

  const size_t total_size =
      ranges::accumulate(ucs2_vec, 0,
                         [](size_t cur, const StringContainer& p) {
                           return cur + std::get<2>(p);
                         });

  if (_used + total_size <= _width) {
    for (size_t i = 0; i < ucs2_vec.size(); ++i) {
      auto [str, font, size] = ucs2_vec[i];
      FontColor* color = seg.segments[i].color == NORMAL_COLOR
                             ? &_colors->foreground
                             : &_colors->fg_accent;

      font->draw_ucs2(_xft_draw, color, str, _height, _used);
      if (seg.action) {
        const uint16_t end = _used + size;
        _areas.push_back({.begin = _used, .end = end, .action = *seg.action});
        _used = end;
      } else {
        _used += size;
      }
    }
  }
}
