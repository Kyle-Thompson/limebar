#pragma once

#include <string>
#include <vector>

#include "bar_color.h"
#include "config.h"
#include "font.h"
#include "types.h"

class SectionPixmap {
 public:
  SectionPixmap(DS::pixmap_t pixmap, BarColors* colors, Fonts* fonts,
                uint16_t width, uint16_t height);
  ~SectionPixmap() = default;
  SectionPixmap(const SectionPixmap&) = delete;
  SectionPixmap(SectionPixmap&&) = delete;
  SectionPixmap& operator=(const SectionPixmap&) = delete;
  SectionPixmap& operator=(SectionPixmap&&) = delete;

  [[nodiscard]] uint16_t size() const { return _used; }
  [[nodiscard]] const DS::pixmap_t& pixmap() const { return _pixmap; }

  void clear();
  void append(const SectionPixmap& rhs);
  void write(const segment_t& seg);

  void click(size_t x, uint8_t button) const;

 private:
  uint16_t _used;
  uint16_t _width, _height;
  DS& _ds;
  BarColors* _colors;
  Fonts* _fonts;
  DS::pixmap_t _pixmap;
  XftDraw* _xft_draw;
  std::vector<area_t> _areas;
};
