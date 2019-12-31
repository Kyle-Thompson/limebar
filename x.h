#pragma once

#include "config.h"
#include "color.h"

#include <bits/stdint-uintn.h>
#include <fontconfig/fontconfig.h>
#include <iostream>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <xcb/xcb.h>
#include <xcb/xcb_xrm.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>


class X {
 public:
  class font_color;
  static X& Instance();

  // TODO: label
  void get_string_resource(const char* query, char **out);
  // TODO: provide better API
  void clear_rect(xcb_drawable_t d, uint16_t width, uint16_t height);
  uint32_t generate_id() { return xcb_generate_id(connection); }
  void     flush() { xcb_flush(connection); }
  void     copy_area(xcb_drawable_t src, xcb_drawable_t dst, int16_t src_x,
                     int16_t dst_x, uint16_t width, uint16_t height);
  void     create_gc(xcb_pixmap_t pixmap, const rgba_t& rgb);
  void     create_pixmap(xcb_pixmap_t pid, xcb_drawable_t drawable,
                         uint16_t width, uint16_t height);
  void     free_pixmap(xcb_pixmap_t pixmap);
  void     create_window(xcb_window_t wid, const rgba_t& rgb, int16_t x,
      int16_t y, uint16_t width, uint16_t height, uint16_t _class,
      xcb_visualid_t visual, uint32_t value_mask, bool reserve_space);
  void     destroy_window(xcb_window_t window);
  void     configure_window(xcb_window_t window, uint16_t value_mask,
                            const void *value_list);
  void     map_window(xcb_window_t window);
  xcb_generic_event_t* wait_for_event();

  // TODO: label
  template <typename T>
  std::optional<std::vector<T>> get_property(Window win, Atom xa_prop_type,
      const char *prop_name);
  std::string   get_window_title(Window win);
  std::vector<Window> get_windows();
  uint32_t      get_current_workspace();
  Window        get_default_root_window();
  Atom          get_intern_atom();
  XVisualInfo*  get_visual_info(long vinfo_mask, XVisualInfo *vinfo_template,
                                int *nitems_return);
  xcb_intern_atom_cookie_t get_atom_by_name(const char* name);
  xcb_intern_atom_reply_t* get_intern_atom_reply(const char* name);

  void set_ewmh_atoms();
  bool connection_has_error();

  xcb_visualid_t get_visual();

  // XFT functions
  XftColor alloc_char_color(const rgba_t& rgb);
  bool     xft_char_exists(XftFont *pub, FcChar32 ucs4);
  FT_UInt  xft_char_index(XftFont *pub, FcChar32 ucs4);
  int      xft_char_width(uint16_t ch);
  void     xft_color_free(XftColor* color);
  XftDraw* xft_draw_create(Drawable drawable);
  void     xft_font_close(XftFont *xft);
  XftFont* xft_font_open_name(_Xconst char *name);
  void     draw_ucs2_string(XftDraw* draw, font_color* color,
                            const std::vector<uint16_t>& str, size_t x);
  
 private:
  X();
  ~X();
  uint8_t get_depth();

  Display            *display;
  xcb_connection_t   *connection;
  xcb_xrm_database_t *database;
  xcb_screen_t       *screen;

  // TODO: use a more optimized hash map
  std::unordered_map<uint16_t, char> xft_char_widths;
  std::shared_mutex _char_widths_mutex;

 public:  // temp
  class font_color {
   public:
    font_color(const rgba_t& color)
      : _color(X::Instance().alloc_char_color(color))
    {}
    ~font_color() {
      X::Instance().xft_color_free(&_color);
    }
    font_color(const font_color& rhs) : _color(rhs._color) {}
    font_color(font_color&&) = delete;
    font_color& operator=(const font_color&) = delete;
    font_color& operator=(font_color&&) = delete;

    XftColor *get() { return &_color; }

   private:
    XftColor _color;
  };

  struct font_t {
    font_t() = default;
    font_t(const char* pattern, int offset, X* x) {
      _x = x;
      if ((xft_ft = _x->xft_font_open_name(pattern))) {
        descent = xft_ft->descent;
        height = xft_ft->ascent + descent;
        this->offset = offset;
      } else {
        std::cerr << "Could not load font " << pattern << "\n";
        exit(EXIT_FAILURE);
      }
    }
    ~font_t() { _x->xft_font_close(xft_ft); }

    bool font_has_glyph(const uint16_t c) {
      return _x->xft_char_exists(xft_ft, (FcChar32) c);
    }

    X* _x;
    XftFont *xft_ft { nullptr };
    int descent { 0 };
    int height { 0 };
    int offset { 0 };
  };

  struct Fonts {
    Fonts() = default;
    void init(X* x) {
      // initialize fonts
      std::transform(FONTS.begin(), FONTS.end(), _fonts.begin(),
          [&](const auto& f) -> font_t {
            const auto& [font, offset] = f;
            return { font, offset, x };
          });

      // to make the alignment uniform, find maximum height
      const int maxh = std::max_element(_fonts.begin(), _fonts.end(),
          [](const auto& l, const auto& r){
            return l.height < r.height;
          })->height;

      // set maximum height to all fonts
      for (auto& font : _fonts)
        font.height = maxh;
    }
    font_t& operator[](size_t index) { return _fonts[index]; }
    font_t& drawable_font(const uint16_t c) {
      // If the end is reached without finding an appropriate font, return nullptr.
      // If the font can draw the character, return it.
      for (auto& font : _fonts) {
        if (font.font_has_glyph(c)) {
          return font;
        }
      }
      return _fonts[0];  // TODO: print error and exit?
    }

    std::array<font_t, FONTS.size()> _fonts;
  };

  Fonts fonts;
 private:  // temp

  xcb_gcontext_t gc_bg;
  xcb_colormap_t colormap;

  Visual *visual_ptr;
  xcb_visualid_t visual;
};


template <typename T>
std::optional<std::vector<T>>
X::get_property(Window win, Atom xa_prop_type, const char *prop_name)
{
  Atom xa_ret_type;
  int ret_format;
  uint64_t ret_nitems;
  uint64_t ret_bytes_after;
  unsigned char *ret_prop;

  Atom xa_prop_name = XInternAtom(display, prop_name, False);

  if (XGetWindowProperty(display, win, xa_prop_name, 0, 1024, False,
          xa_prop_type, &xa_ret_type, &ret_format, &ret_nitems,
          &ret_bytes_after, &ret_prop)
      != Success) {
    return std::nullopt;
  }

  if (xa_ret_type != xa_prop_type || ret_bytes_after > 0) {
    XFree(ret_prop);
    return std::nullopt;
  }

  return std::vector<T>((T *)ret_prop,
                        (T *)(ret_prop + (ret_nitems * sizeof(T))));
}


// helpers


xcb_atom_t get_atom(xcb_connection_t *conn, const char *name);
xcb_connection_t *get_connection();
