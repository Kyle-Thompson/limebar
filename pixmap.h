#pragma once

#include <string>
#include <vector>

#include "bar_color.h"
#include "config.h"
#include "font.h"
#include "types.h"

// TODO: create a wrapper class that only allows appending and not clearing to
// limit access to what modules can do when getting a ModulePixmap
class ModulePixmap {
 public:
  ModulePixmap(DS::pixmap_t pixmap, BarColors* colors, Fonts* fonts,
               uint16_t width, uint16_t height);
  ~ModulePixmap() = default;
  ModulePixmap(const ModulePixmap&) = delete;
  ModulePixmap(ModulePixmap&&) = delete;
  ModulePixmap& operator=(const ModulePixmap&) = delete;
  ModulePixmap& operator=(ModulePixmap&&) = delete;

  [[nodiscard]] uint16_t size() const { return _used; }
  [[nodiscard]] const DS::pixmap_t& pixmap() const { return _pixmap; }

  void clear();
  void append(const ModulePixmap& rhs);
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
