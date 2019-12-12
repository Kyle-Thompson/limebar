#include "window.h"
#include <cstddef>
#include <vector>

enum {
  NET_WM_WINDOW_TYPE,
  NET_WM_WINDOW_TYPE_DOCK,
  NET_WM_DESKTOP,
  NET_WM_STRUT_PARTIAL,
  NET_WM_STRUT,
  NET_WM_STATE,
  NET_WM_STATE_STICKY,
  NET_WM_STATE_ABOVE,
};

static constexpr size_t size = 8;
static constexpr std::array<const char *, size> atom_names {
  "_NET_WM_WINDOW_TYPE",
  "_NET_WM_WINDOW_TYPE_DOCK",
  "_NET_WM_DESKTOP",
  "_NET_WM_STRUT_PARTIAL",
  "_NET_WM_STRUT",
  "_NET_WM_STATE",
  // Leave those at the end since are batch-set
  "_NET_WM_STATE_STICKY",
  "_NET_WM_STATE_ABOVE",
};
std::array<xcb_atom_t, size> atom_list;

BarWindow::BarWindow(size_t x, size_t y, size_t w, size_t h)
  : _x(X::Instance()), _window(_x.generate_id()), _pixmap(_x.generate_id())
  , _origin_x(x), _origin_y(y), _width(w) , _height(h)
{
  // TODO: investigate if this should be redone per this leftover comment:
  // As suggested fetch all the cookies first (yum!) and then retrieve the
  // atoms to exploit the async'ness
  std::transform(atom_names.begin(), atom_names.end(), atom_list.begin(),
      [this](auto name){
        xcb_intern_atom_reply_t *atom_reply =
            _x.get_intern_atom_reply(name);
        if (!atom_reply) {
          fprintf(stderr, "atom reply failed.\n");
          exit(EXIT_FAILURE);
        }
        auto ret = atom_reply->atom;
        free(atom_reply);  // TODO: use unique_ptr
        return ret;
      });

  // create a window with width and height
  const uint32_t mask[] { *_x.bgc.val(), *_x.bgc.val(), FORCE_DOCK,
    XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE,
    _x.get_colormap() };

  _x.create_window(_window, _origin_x, _origin_y, _width, _height,
      XCB_WINDOW_CLASS_INPUT_OUTPUT, _x.get_visual(),
      XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT |
          XCB_CW_EVENT_MASK | XCB_CW_COLORMAP,
      mask);

  _x.create_pixmap(_pixmap, _window, _width, _height);

  int strut[12] = {0};
  // TODO: Find a better way of determining if this is a top-bar
  if (_origin_y == 0) {
    strut[2] = BAR_HEIGHT;
    strut[8] = _origin_x;
    strut[9] = _origin_x + _width;
  } else {
    strut[3]  = BAR_HEIGHT;
    strut[10] = _origin_x;
    strut[11] = _origin_x + _width;
  }

  _x.change_property(XCB_PROP_MODE_REPLACE, _window,
      atom_list[NET_WM_WINDOW_TYPE], XCB_ATOM_ATOM, 32, 1,
      &atom_list[NET_WM_WINDOW_TYPE_DOCK]);
  _x.change_property(XCB_PROP_MODE_APPEND, _window,
      atom_list[NET_WM_STATE], XCB_ATOM_ATOM, 32, 2,
      &atom_list[NET_WM_STATE_STICKY]);
  _x.change_property(XCB_PROP_MODE_REPLACE, _window,
      atom_list[NET_WM_DESKTOP], XCB_ATOM_CARDINAL, 32, 1,
      (const uint32_t []) { 0u - 1u } );
  _x.change_property(XCB_PROP_MODE_REPLACE, _window,
      atom_list[NET_WM_STRUT_PARTIAL], XCB_ATOM_CARDINAL, 32, 12, strut);
  _x.change_property(XCB_PROP_MODE_REPLACE, _window, atom_list[NET_WM_STRUT],
      XCB_ATOM_CARDINAL, 32, 4, strut);
  _x.change_property(XCB_PROP_MODE_REPLACE, _window, XCB_ATOM_WM_NAME,
      XCB_ATOM_STRING, 8, 3, "bar");
  _x.change_property(XCB_PROP_MODE_REPLACE, _window, XCB_ATOM_WM_CLASS,
      XCB_ATOM_STRING, 8, 12, "lemonbar\0Bar");

  _x.map_window(_window);
  _x.create_gc(_pixmap);
  _x.fill_rect(_pixmap, GC_CLEAR, 0, 0, _width, _height);

  // Make sure that the window really gets in the place it's supposed to be
  // Some WM such as Openbox need this
  const uint32_t xy[] {
      static_cast<uint32_t>(_origin_x), static_cast<uint32_t>(_origin_y) };
  _x.configure_window(_window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, xy);

  // Set the WM_NAME atom to the user specified value
  if constexpr (WM_NAME != nullptr)
    _x.change_property(XCB_PROP_MODE_REPLACE, _window,
        XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(WM_NAME), WM_NAME);

  // set the WM_CLASS atom instance to the executable name
  if (WM_CLASS.size()) {
    constexpr int size = WM_CLASS.size() + 6;
    char wm_class[size] = {0};

    // WM_CLASS is nullbyte seperated: WM_CLASS + "\0Bar\0"
    strncpy(wm_class, WM_CLASS.data(), WM_CLASS.size());
    strcpy(wm_class + WM_CLASS.size(), "\0Bar");

    _x.change_property(XCB_PROP_MODE_REPLACE, _window,
        XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, size, wm_class);
  }

  _x.fill_rect(_pixmap, GC_CLEAR, 0, 0, _width, _height);
  _x.update_gc();  // TODO: can we remove this?
}

void
BarWindow::update_left(const ModulePixmap& pixmap) {
  if (pixmap.size() + _offset_left <= _width) {
    _x.copy_area(pixmap.pixmap(), _window, 0, _offset_left, pixmap.size(),
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
    _x.copy_area(pixmap.pixmap(), _window, 0,
        _width / 2 - middle_offset, middle_offset * 2, _height);
  }
}

void
BarWindow::update_right(const ModulePixmap& pixmap) {
  // TODO: check if this would overflow onto left modules.
  // we don't care about middle as they have last priority.

  _x.copy_area(pixmap.pixmap(), _window, 0,
               _width - _offset_right - pixmap.size(), pixmap.size(), _height);
  _offset_right += pixmap.size();
}
