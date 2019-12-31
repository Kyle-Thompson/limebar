#pragma once

#include "color.h"
#include "pixmap.h"
#include "x.h"

#include <array>
#include <cstddef>  // size_t
#include <memory>
#include <xcb/xproto.h>

template <typename DS>
class BarWindow {
 public:
  BarWindow(BarColors<DS>&& colors, size_t x, size_t y, size_t w, size_t h);

  ~BarWindow() {
    _x.destroy_window(_window);
    _x.free_pixmap(_pixmap);
  }

  void clear() {
    _offset_left = _offset_right = 0;
    _x.copy_area(_pixmap, _window, 0, 0, _width, _height);
  }

  void render() {
    _x.flush();
  }

  void update_left(const ModulePixmap<DS>& pixmap);
  void update_middle(const ModulePixmap<DS>& pixmap);
  void update_right(const ModulePixmap<DS>& pixmap);

  [[nodiscard]] xcb_pixmap_t generate_pixmap() const {
    xcb_pixmap_t pixmap = _x.generate_id();
    _x.create_pixmap(pixmap, _window, _width, _height);
    return pixmap;
  }

  [[nodiscard]] auto generate_mod_pixmap() {
    return ModulePixmap<DS>(_window, &_colors, _width, _height);
  }

 private:
  X& _x;
  xcb_window_t _window;
  xcb_pixmap_t _pixmap;
  BarColors<DS> _colors;
  size_t _origin_x, _origin_y, _width, _height;
  size_t _offset_left { 0 }, _offset_right { 0 };
};


template <typename DS>
BarWindow<DS>::BarWindow(BarColors<DS>&& colors, size_t x, size_t y, size_t w,
                         size_t h)
  : _x(X::Instance()), _window(_x.generate_id()), _pixmap(_x.generate_id())
  , _colors(std::move(colors)), _origin_x(x), _origin_y(y), _width(w)
  , _height(h)
{
  // create a window with width and height
  _x.create_window(_window, colors.background, _origin_x, _origin_y, _width,
      _height, XCB_WINDOW_CLASS_INPUT_OUTPUT, _x.get_visual(),
      XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT |
          XCB_CW_EVENT_MASK | XCB_CW_COLORMAP, true);

  _x.create_pixmap(_pixmap, _window, _width, _height);
  _x.create_gc(_pixmap, colors.background);
  _x.clear_rect(_pixmap, _width, _height);
}

template <typename DS>
void
BarWindow<DS>::update_left(const ModulePixmap<DS>& pixmap) {
  if (pixmap.size() + _offset_left <= _width) {
    _x.copy_area(pixmap.pixmap(), _window, 0, _offset_left, pixmap.size(),
                 _height);
    _offset_left += pixmap.size();
  }
}

template <typename DS>
void
BarWindow<DS>::update_middle(const ModulePixmap<DS>& pixmap) {
  // TODO(1): remove
  size_t largest_offset = std::max(_offset_left, _offset_right);
  if (largest_offset < _width / 2) {
    size_t middle_offset = std::min(
        _width / 2 - largest_offset,
        static_cast<size_t>(pixmap.size()) / 2);
    _x.copy_area(pixmap.pixmap(), _window, 0,
        _width / 2 - middle_offset, middle_offset * 2, _height);
  }
}

template <typename DS>
void
BarWindow<DS>::update_right(const ModulePixmap<DS>& pixmap) {
  // TODO: check if this would overflow onto left modules.
  // we don't care about middle as they have last priority.

  _x.copy_area(pixmap.pixmap(), _window, 0,
               _width - _offset_right - pixmap.size(), pixmap.size(), _height);
  _offset_right += pixmap.size();
}
