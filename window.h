#pragma once

#include <xcb/xproto.h>

#include <array>
#include <cstddef>  // size_t
#include <memory>

#include "bar_color.h"
#include "config.h"
#include "font.h"
#include "pixmap.h"

class BarWindow {
 public:
  BarWindow(BarColors&& colors, Fonts&& fonts, size_t x, size_t y, size_t w,
            size_t h);

  ~BarWindow() {
    _ds.destroy_window(_window);
    _ds.free_pixmap(_pixmap);
  }

  // Resets the underlying pixelmap used for drawing to the window. Has no
  // effect on the window itself.
  void reset() {
    _ds.clear_rect(_pixmap, _width, _height);
  }

  void render() {
    _ds.copy_area(_pixmap, _window, 0, 0, _width, _height);
    _offset_left = _offset_right = 0;
  }

  void update_left(const ModulePixmap& pixmap);
  void update_middle(const ModulePixmap& pixmap);
  void update_right(const ModulePixmap& pixmap);

  [[nodiscard]] xcb_pixmap_t generate_pixmap() const {
    xcb_pixmap_t pixmap = _ds.generate_id();
    _ds.create_pixmap(pixmap, _window, _width, _height);
    return pixmap;
  }

  [[nodiscard]] auto generate_mod_pixmap() {
    return ModulePixmap(_window, &_colors, &_fonts, _width, _height);
  }

 private:
  DS& _ds;
  xcb_window_t _window;
  xcb_pixmap_t _pixmap;
  BarColors _colors;
  Fonts _fonts;
  size_t _origin_x, _origin_y, _width, _height;
  size_t _offset_left{0}, _offset_right{0};
};
