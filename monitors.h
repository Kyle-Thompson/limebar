#include <vector>
#include <xcb/xproto.h>

#include "color.h"

struct monitor_t {
  // TODO: simplify constructor with internal references to singletons
  monitor_t(int x, int y, int width, int height, xcb_visualid_t visual, rgba_t bgc, xcb_colormap_t colormap);

  int _x, _y, _width;
  xcb_window_t _window;
  xcb_pixmap_t _pixmap;
};


struct Monitors {
  ~Monitors();
  void init(std::vector<xcb_rectangle_t>& rects, xcb_visualid_t visual, rgba_t bgc, xcb_colormap_t colormap);

  std::vector<monitor_t>::iterator begin() { return _monitors.begin(); }
  std::vector<monitor_t>::iterator end()   { return _monitors.end(); }
  std::vector<monitor_t>::const_iterator cbegin() { return _monitors.cbegin(); }
  std::vector<monitor_t>::const_iterator cend()   { return _monitors.cend(); }

  std::vector<monitor_t> _monitors;
};
