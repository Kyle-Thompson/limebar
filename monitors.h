#pragma once

#include <vector>
#include <xcb/xproto.h>

#include "color.h"
#include "fonts.h"


struct area_t {
  uint16_t begin;
  uint16_t end;
  bool active:1;
  int8_t align:3;
  uint8_t button:3;
  xcb_window_t window;
  char *cmd;
};

struct monitor_t {
  // TODO: simplify constructor with internal references to singletons
  monitor_t(int x, int y, int width, int height, xcb_visualid_t visual, rgba_t bgc, xcb_colormap_t colormap);

  int shift(int x, int align, int ch_width);
  void draw_lines(int x, int w);
  void draw_shift(int x, int align, int w);
  int draw_char(font_t *cur_font, int x, int align, FcChar16 ch);

  int _x, _y, _width;
  xcb_window_t _window;
  xcb_pixmap_t _pixmap;
};


struct Monitors {
  static Monitors* Instance();

  void init(std::vector<xcb_rectangle_t>& rects, xcb_visualid_t visual, rgba_t bgc, xcb_colormap_t colormap);

  std::optional<area_t> area_get(xcb_window_t win, const int btn, const int x);
  void area_shift (xcb_window_t win, const int align, int delta);
  bool area_add (char *str, const char *optend, char **end, monitor_t *mon,
      const uint16_t x, const int8_t align, const uint8_t button);

  std::vector<monitor_t>::iterator begin() { return _monitors.begin(); }
  std::vector<monitor_t>::iterator end()   { return _monitors.end(); }
  std::vector<monitor_t>::const_iterator cbegin() { return _monitors.cbegin(); }
  std::vector<monitor_t>::const_iterator cend()   { return _monitors.cend(); }

  std::vector<area_t> _areas;
  std::vector<monitor_t> _monitors;

 private:
  Monitors() = default;
  ~Monitors();

  static Monitors* instance;
};

void set_attribute(const char modifier, const char attribute);
