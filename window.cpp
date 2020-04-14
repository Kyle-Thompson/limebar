#include "window.h"

#include "config.h"
#include "types.h"


BarWindow::BarWindow(BarColors&& colors, Fonts&& fonts, rectangle_t rect)
    : _ds(DS::Instance())
    , _window(_ds.create_window(rect, colors.background, true))
    , _pixmap(_window.create_pixmap())
    , _colors(std::move(colors))
    , _fonts(std::move(fonts))
    , _width(rect.width)
    , _height(rect.height) {
  _window.create_gc(colors.background);
  _pixmap.clear();
}

std::pair<uint16_t, uint16_t>
BarWindow::update_left(const SectionPixmap& pixmap) {
  if (pixmap.size() + _offset_left <= _width) {
    _pixmap.copy_from(pixmap.pixmap(), {0, 0},
                      {static_cast<int16_t>(_offset_left), 0}, pixmap.size(),
                      _height);
    _offset_left += pixmap.size();
    return {0, pixmap.size()};
  }
  return {0, 0};
}

std::pair<uint16_t, uint16_t>
BarWindow::update_middle(const SectionPixmap& pixmap) {
  uint16_t largest_offset = std::max(_offset_left, _offset_right);
  uint16_t half_width = _width / 2;
  if (largest_offset < half_width) {
    uint16_t middle_offset = std::min<uint16_t>(half_width - largest_offset,
                                                pixmap.size() / 2);
    _pixmap.copy_from(pixmap.pixmap(), {0, 0},
                      {static_cast<int16_t>(half_width - middle_offset), 0},
                      middle_offset * 2, _height);
    return {half_width - middle_offset, half_width + middle_offset};
  }
  return {0, 0};
}

std::pair<uint16_t, uint16_t>
BarWindow::update_right(const SectionPixmap& pixmap) {
  _pixmap.copy_from(
      pixmap.pixmap(), {0, 0},
      {static_cast<int16_t>(_width - _offset_right - pixmap.size()), 0},
      pixmap.size(), _height);
  _offset_right += pixmap.size();
  return {_width - _offset_right, _width};
}
