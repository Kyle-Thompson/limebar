#include "config.h"
#include "window.h"


BarWindow::BarWindow(BarColors&& colors,
    Fonts&& fonts, size_t x, size_t y, size_t w, size_t h)
  : _ds(DS::Instance()), _window(_ds.generate_id()), _pixmap(_ds.generate_id())
  , _colors(std::move(colors)), _fonts(std::move(fonts))
  , _origin_x(x), _origin_y(y), _width(w) , _height(h)
{
  // create a window with width and height
  _ds.create_window(_window, colors.background, _origin_x, _origin_y, _width,
      _height, XCB_WINDOW_CLASS_INPUT_OUTPUT, _ds.get_visual(),
      XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT |
          XCB_CW_EVENT_MASK | XCB_CW_COLORMAP, true);

  _ds.create_pixmap(_pixmap, _window, _width, _height);
  _ds.create_gc(_pixmap, colors.background);
  _ds.clear_rect(_pixmap, _width, _height);
}

void
BarWindow::update_left(const ModulePixmap& pixmap) {
  if (pixmap.size() + _offset_left <= _width) {
    _ds.copy_area(pixmap.pixmap(), _window, 0, _offset_left, pixmap.size(),
                 _height);
    _offset_left += pixmap.size();
  }
}

void
BarWindow::update_middle(const ModulePixmap& pixmap) {
  // TODO(1): remove
  size_t largest_offset = std::max(_offset_left, _offset_right);
  if (largest_offset < _width / 2) {
    size_t middle_offset = std::min(
        _width / 2 - largest_offset,
        static_cast<size_t>(pixmap.size()) / 2);
    _ds.copy_area(pixmap.pixmap(), _window, 0,
        _width / 2 - middle_offset, middle_offset * 2, _height);
  }
}

void
BarWindow::update_right(const ModulePixmap& pixmap) {
  // TODO: check if this would overflow onto left modules.
  // we don't care about middle as they have last priority.

  _ds.copy_area(pixmap.pixmap(), _window, 0,
                _width - _offset_right - pixmap.size(), pixmap.size(), _height);
  _offset_right += pixmap.size();
}
