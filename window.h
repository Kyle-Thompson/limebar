#pragma once

#include <array>
#include <cstddef>  // size_t
#include <memory>

#include "bar_color.h"
#include "config.h"
#include "font.h"
#include "pixmap.h"
#include "types.h"

class BarWindow {
 public:
  BarWindow(BarColors&& colors, Fonts&& fonts, rectangle_t rect);
  ~BarWindow() = default;

  BarWindow(const BarWindow&) = delete;
  BarWindow(BarWindow&&) = default;
  BarWindow& operator=(const BarWindow&) = delete;
  BarWindow& operator=(BarWindow&&) = delete;

  // Resets the underlying pixelmap used for drawing to the window. Has no
  // effect on the window itself.
  void reset() { _pixmap.clear(); }

  void render() {
    _window.copy_from(_pixmap, {0, 0}, {0, 0}, _width, _height);
    _offset_left = _offset_right = 0;
  }

  std::pair<uint16_t, uint16_t> update_left(const SectionPixmap& pixmap);
  std::pair<uint16_t, uint16_t> update_middle(const SectionPixmap& pixmap);
  std::pair<uint16_t, uint16_t> update_right(const SectionPixmap& pixmap);

  [[nodiscard]] DS::pixmap_t generate_pixmap() const {
    return _window.create_pixmap();
  }

  [[nodiscard]] auto generate_mod_pixmap() {
    return SectionPixmap(_window.create_pixmap(), &_colors, &_fonts, _width,
                         _height);
  }

 private:
  DS& _ds;
  DS::window_t _window;
  DS::pixmap_t _pixmap;
  BarColors _colors;
  Fonts _fonts;
  uint16_t _width, _height;
  uint16_t _offset_left{0}, _offset_right{0};
};
