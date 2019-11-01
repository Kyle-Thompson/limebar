#pragma once

#include "x.h"

#include <array>
#include <cstddef>  // size_t
#include <xcb/xproto.h>

class BarWindow {
 public:
  BarWindow(X& x11, size_t x, size_t y, size_t w, size_t h);

  ~BarWindow() {
    _x.destroy_window(_window);
    _x.free_pixmap(_pixmap);
  }

  void clear() {
    _x.fill_rect(_pixmap, GC_CLEAR, 0, 0, width, height);
  }

  void render() {
    _x.copy_area(_pixmap, _window, 0, 0, width);
    _x.flush();
  }

 private:
  X& _x;
  xcb_window_t _window;
  xcb_pixmap_t _pixmap;
  size_t origin_x, origin_y, width, height;
};
