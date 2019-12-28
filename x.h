#pragma once

#include "config.h"
#include "color.h"
#include "enums.h"

#include <bits/stdint-uintn.h>
#include <fontconfig/fontconfig.h>
#include <iostream>
#include <mutex>
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
  static X& Instance();

  // TODO: label
  void get_string_resource(const char* query, char **out);
  void change_property(uint8_t mode, xcb_window_t window, xcb_atom_t property,
      xcb_atom_t type, uint8_t format, uint32_t data_len, const void *data);
  // TODO: provide better API
  void fill_rect(xcb_drawable_t d, uint32_t gc_index, int16_t x, int16_t y,
                 uint16_t width, uint16_t height);
  uint32_t generate_id() { return xcb_generate_id(connection); }
  void     flush() { xcb_flush(connection); }
  void     update_gc();
  void     copy_area(xcb_drawable_t src, xcb_drawable_t dst, int16_t src_x,
                     int16_t dst_x, uint16_t width, uint16_t height);
  void     create_gc(xcb_pixmap_t pixmap);
  void     create_pixmap(xcb_pixmap_t pid, xcb_drawable_t drawable,
                         uint16_t width, uint16_t height);
  void     free_pixmap(xcb_pixmap_t pixmap);
  void     create_window(xcb_window_t wid, int16_t x, int16_t y, uint16_t width,
      uint16_t height, uint16_t _class, xcb_visualid_t visual,
      uint32_t value_mask, const void *value_list);
  void     destroy_window(xcb_window_t window);
  void     configure_window(xcb_window_t window, uint16_t value_mask,
                            const void *value_list);
  void     map_window(xcb_window_t window);
  xcb_generic_event_t* wait_for_event();

  // TODO: label
  template <typename T>
  T*            get_property(Window win, Atom xa_prop_type,
                             const char *prop_name, unsigned long *size);
  std::string   get_window_title(Window win);
  std::vector<Window> get_windows();
  unsigned long get_current_workspace();
  Window        get_default_root_window();
  Atom          get_intern_atom();
  XVisualInfo*  get_visual_info(long vinfo_mask, XVisualInfo *vinfo_template,
                                int *nitems_return);
  xcb_intern_atom_cookie_t get_atom_by_name(const char* name);
  xcb_intern_atom_reply_t* get_intern_atom_reply(const char* name);

  void set_ewmh_atoms();
  void set_event_queue_order(enum XEventQueueOwner owner);
  bool connection_has_error();
  auto get_gc() { return gc; }
  auto get_colormap() { return colormap; }

  xcb_visualid_t get_visual();

  // XFT functions
  bool     xft_char_exists(XftFont *pub, FcChar32 ucs4);
  FT_UInt  xft_char_index(XftFont *pub, FcChar32 ucs4);
  int      xft_char_width(uint16_t ch);
  void     xft_color_free(XftColor* color);
  Visual*  xft_default_visual(int screen);
  XftDraw* xft_draw_create(Drawable drawable);
  void     xft_font_close(XftFont *xft);
  XftFont* xft_font_open_name(_Xconst char *name);
  void     draw_ucs2_string(XftDraw* draw, const std::vector<uint16_t>& str,
                            size_t x);
  void     draw_ucs2_string_accent(XftDraw* draw,
                                   const std::vector<uint16_t>& str, size_t x);

  // TODO: make these private
  rgba_t fgc, bgc, ugc;
  rgba_t accent;  // TODO: clean up color management
  XftDraw *xft_draw { nullptr };
  
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

  xcb_gcontext_t gc[GC_MAX];
  XftColor fg_color;
  XftColor acc_color;
  xcb_colormap_t colormap;

  Visual *visual_ptr;
  xcb_visualid_t visual;
};


// helpers


xcb_atom_t get_atom(xcb_connection_t *conn, const char *name);
xcb_connection_t *get_connection();
