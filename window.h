#pragma once

#include "pixmap.h"
#include "x.h"

#include <array>
#include <cstddef>  // size_t
#include <vector>
#include <xcb/xproto.h>

class BarWindow {
 public:
  BarWindow(size_t x, size_t y, size_t w, size_t h);

  ~BarWindow() {
    _x.destroy_window(_window);
    _x.free_pixmap(_pixmap);
  }

  void clear() {
    _offset_left = _offset_right = 0;
    /* _middle_pixmap.clear(); */
    _x.copy_area(_pixmap, _window, 0, 0, _width, _height);
  }

  void render() {
    // TODO(1): uncomment
    /* size_t largest_offset = std::max(_offset_left, _offset_right); */
    /* if (largest_offset < _width / 2) { */
    /*   size_t middle_offset = std::min( */
    /*       _width / 2 - largest_offset, */
    /*       static_cast<size_t>(_middle_pixmap.size()) / 2); */
    /*   _x.copy_area(_middle_pixmap.pixmap(), _window, 0, */
    /*       _width / 2 - middle_offset, middle_offset * 2, _height); */
    /* } */

    _x.flush();
  }

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
  size_t _offset_left { 0 }, _offset_right { 0 };
  ModulePixmap _middle_pixmap;
};
