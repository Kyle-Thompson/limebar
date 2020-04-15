// clang-format off
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
// clang-format on

#include "pixmap.h"


static ucs2
utf8_to_ucs2(const std::string& text) {
  ucs2 str;

  for (uint8_t* utf = (uint8_t*)text.c_str(); utf != (uint8_t*)&*text.end();) {
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


SectionPixmap::SectionPixmap(DS::pixmap_t pixmap, BarColors* colors,
                             uint16_t width, uint16_t height)
    : _used(0)
    , _width(width)
    , _height(height)
    , _ds(DS::Instance())
    , _colors(colors)
    , _pixmap(std::move(pixmap))
    , _xft_draw(_pixmap.create_xft_draw()) {
}


/** clear
 * Reset the class to its original state.
 */
void
SectionPixmap::clear() {
  _used = 0;
  _areas = std::vector<area_t>();
  _pixmap.clear();
}


/** click
 * Run the action for the range containing x in _areas.
 */
void
SectionPixmap::click(int16_t x, uint8_t button) const {
  for (const auto& area : _areas) {
    if (x >= area.begin && x <= area.end) {
      area.action(button);
      break;
    }
  }
}


/** write
 * Given a segment, write its contents to the underlying pixelmap and add the
 * corresponding action to the vector of areas iff the entire segment can be
 * written.
 */
void
SectionPixmap::write(const segment_t& seg) {
  using FontType = typename DS::font_t;
  using StringContainer = std::tuple<ucs2, FontType*, size_t>;

  const auto strings =
      seg.segments |
      ranges::views::transform([this](const auto& s) -> StringContainer {
        ucs2 str = utf8_to_ucs2(s.str);
        FontType* font = _ds.get_drawable_font(str[0]);
        return {str, font, font->string_size(str)};
      });

  const decltype(_used) total_size = ranges::accumulate(
      strings, decltype(total_size){},
      [](decltype(total_size) cur, const StringContainer& p) {
        return cur + std::get<2>(p);
      });

  if (_used + total_size > _width) {
    return;
  }

  if (seg.action) {
    const uint16_t end = _used + total_size;
    _areas.push_back({.begin = _used, .end = end, .action = *seg.action});
  }
  for (auto&& [string, text_seg] : ranges::views::zip(strings, seg.segments)) {
    const auto& [str, font, size] = string;
    FontColor* color = text_seg.color == NORMAL_COLOR ? &_colors->foreground
                                                      : &_colors->fg_accent;
    font->draw_ucs2(_xft_draw, color, str, _height, _used);
    _used += size;
  }
}


/** pipe operator
 * write() the segments from a generator one at a time.
 */
void
operator|(cppcoro::generator<const segment_t&> generator,
          SectionPixmap& pixmap) {
  for (const auto& seg : generator) {
    pixmap.write(seg);
  }
}
