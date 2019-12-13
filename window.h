#pragma once

#include <xcb/xproto.h>

#include <array>
#include <cstddef>  // size_t
#include <vector>

#include "pixmap.h"
#include "x.h"

class BarWindow {
 public:
  BarWindow(size_t x, size_t y, size_t w, size_t h);

  ~BarWindow() {
    _x.destroy_window(_window);
    _x.free_pixmap(_pixmap);
  }

  void clear() {
    _offset_left = _offset_right = 0;
    _x.copy_area(_pixmap, _window, 0, 0, _width, _height);
  }

  void render() { _x.flush(); }

  void update_left(const ModulePixmap& pixmap);
  void update_middle(const ModulePixmap& pixmap);
  void update_right(const ModulePixmap& pixmap);

  xcb_pixmap_t generate_pixmap() const {
    xcb_pixmap_t pixmap = _x.generate_id();
    _x.create_pixmap(pixmap, _window, _width, _height);
    return pixmap;
  }

  ModulePixmap generate_mod_pixmap() const {
    return ModulePixmap(_window, _width, _height);
  }

 private:
  X& _x;
  xcb_window_t _window;
  xcb_pixmap_t _pixmap;
  size_t _origin_x, _origin_y, _width, _height;
  size_t _offset_left{0}, _offset_right{0};
};
