#pragma once

#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>
#include <fontconfig/fontconfig.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_xrm.h>
#include <xcb/xproto.h>

#include <cppcoro/generator.hpp>
#include <iostream>
#include <mutex>
#include <numeric>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "color.h"
#include "types.h"


class X11 {
 public:
  class font_color_t;
  class font_t;
  class window_t;
  class pixmap_t;  // created through window_t
  class rdb_t;

  static X11& Instance();
  static void init();

  // cannot be copied or moved
  X11(const X11&) = delete;
  X11(X11&&) = delete;
  X11& operator=(const X11&) = delete;
  X11& operator=(X11&&) = delete;
  ~X11();

  // actions
  void activate_window(xcb_window_t window);
  void switch_desktop(uint8_t desktop);

  // resource creators
  [[nodiscard]] auto create_font_color(const rgba_t& rgb) -> font_color_t;
  [[nodiscard]] auto create_font(const char* pattern, int offset = 0) -> font_t;
  [[nodiscard]] auto create_window(rectangle_t dim, const rgba_t& rgb,
                                   bool reserve_space) -> window_t;
  [[nodiscard]] auto create_resource_database() -> rdb_t;

  // events
  // TODO: abstract this away from xcb_* details
  std::unique_ptr<xcb_generic_event_t, decltype(std::free)*> wait_for_event();

  // queries
  [[nodiscard]] auto get_windows() -> cppcoro::generator<xcb_window_t>;
  [[nodiscard]] auto get_active_window() -> xcb_window_t;
  [[nodiscard]] auto get_window_title(xcb_window_t win) -> std::string;
  [[nodiscard]] auto get_workspace_names() -> cppcoro::generator<std::string>;
  [[nodiscard]] auto get_current_workspace() -> uint32_t;
  [[nodiscard]] auto get_workspace_of_window(xcb_window_t window)
      -> std::optional<uint32_t>;

 private:
  X11();

  // query internal X state
  uint8_t get_depth() {
    return (_xlib_visual == _screen->root_visual) ? XCB_COPY_FROM_PARENT : 32;
  }
  auto get_xlib_visual() -> std::pair<xcb_visualid_t, Visual*>;
  auto get_atom_by_name(const char* name) -> xcb_intern_atom_cookie_t;
  uint32_t generate_id() { return xcb_generate_id(_connection); }

  Display* _display;
  xcb_connection_t* _connection;
  xcb_ewmh_connection_t _ewmh;
  xcb_screen_t* _screen;

  xcb_gcontext_t _gc_bg;
  xcb_colormap_t _colormap;

  Visual* _xlib_visual_ptr;
  xcb_visualid_t _xlib_visual;
};


class X11::font_color_t {
 public:
  ~font_color_t();
  font_color_t(const font_color_t& rhs) = default;
  font_color_t(font_color_t&&) = default;
  font_color_t& operator=(const font_color_t&) = default;
  font_color_t& operator=(font_color_t&&) = default;

  XftColor* get() { return &_color; }

 private:
  friend X11;
  font_color_t(X11* x, const rgba_t& rgb);

  X11* _x;
  XftColor _color;
};


class X11::font_t {
  // TODO: optimize for monospace fonts

 public:
  struct glyph_t {
    FT_UInt id;
    XGlyphInfo info;
  };

  ~font_t();
  font_t(const font_t&) = delete;
  font_t(font_t&&) = delete;
  font_t& operator=(const font_t&) = delete;
  font_t& operator=(font_t&&) = delete;

  void draw_ucs2(XftDraw* draw, font_color_t* color, const ucs2& str,
                 uint16_t height, size_t x);

  bool has_glyph(uint16_t ch);
  size_t string_size(const ucs2& str);

  [[nodiscard]] int descent() const { return _descent; }
  [[nodiscard]] int height() const { return _height; }
  [[nodiscard]] int offset() const { return _offset; }

  void height(int h) { _height = h; }

 private:
  friend X11;
  font_t(Display* dpy, const char* pattern, int offset = 0);

  using glyph_map_t = std::unordered_map<uint16_t, glyph_t>;
  using glyph_map_itr = glyph_map_t::const_iterator;

  glyph_map_itr get_glyph(uint16_t ch);
  glyph_t create_glyph(uint16_t ch);
  uint16_t char_width(uint16_t ch);

  int _descent{0};
  int _height{0};
  int _offset{0};

  Display* _display;
  XftFont* _xft_ft;
  std::shared_mutex _mu;
  glyph_map_t _glyph_map;
};


class X11::window_t {
 public:
  ~window_t();
  window_t(const window_t&) = delete;
  window_t(window_t&&) noexcept;
  window_t& operator=(const window_t&) = delete;
  window_t& operator=(window_t&&) = delete;

  void make_visible();
  void configure(uint16_t mask, const void* list);

  void copy_from(const pixmap_t& rhs, coordinate_t src, coordinate_t dst,
                 uint16_t width, uint16_t height);

  pixmap_t create_pixmap() const;
  void create_gc(const rgba_t& rgb) const;

 private:
  friend X11;  // TODO temporary
  explicit window_t(X11* x, uint16_t width, uint16_t height);

  X11* _x;
  xcb_window_t _id;
  uint16_t _width;
  uint16_t _height;
};


class X11::pixmap_t {
 public:
  ~pixmap_t();
  pixmap_t(const pixmap_t&) = delete;
  pixmap_t(pixmap_t&&) noexcept;
  pixmap_t& operator=(const pixmap_t&) = delete;
  pixmap_t& operator=(pixmap_t&&) = delete;

  void clear();
  void copy_from(const pixmap_t& rhs, coordinate_t src, coordinate_t dst,
                 uint16_t width, uint16_t height);
  [[nodiscard]] XftDraw* create_xft_draw() const;

 private:
  friend window_t;
  pixmap_t(X11* x, xcb_drawable_t d, uint16_t width, uint16_t height);

  X11* _x;
  xcb_pixmap_t _id;
  uint16_t _width;
  uint16_t _height;
};


class X11::rdb_t {
 public:
  explicit rdb_t(X11* x);
  ~rdb_t();

  rdb_t(const rdb_t&) = delete;
  rdb_t(rdb_t&&) noexcept;
  rdb_t& operator=(const rdb_t&) = delete;
  rdb_t& operator=(rdb_t&&) noexcept;

  template <typename T>
  T get(const char* query);

 private:
  xcb_xrm_database_t* _db;
};


// helpers

xcb_atom_t get_atom(xcb_connection_t* conn, const char* name);
xcb_connection_t* get_connection();
