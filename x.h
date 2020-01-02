#pragma once

#include "config.h"
#include "color.h"

#include <bits/stdint-uintn.h>
#include <fontconfig/fontconfig.h>
#include <iostream>
#include <mutex>
#include <numeric>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <xcb/xcb.h>
#include <xcb/xcb_xrm.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>


using ucs2 = std::vector<uint16_t>;

class X {
 public:
  class font_color;
  struct font_t;
  static X& Instance();

  // TODO: label
  std::string get_string_resource(const char* query);
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

  xcb_visualid_t get_visual();

  // XFT functions
  XftColor alloc_char_color(const rgba_t& rgb);
  void     xft_color_free(XftColor* color);
  XftDraw* xft_draw_create(Drawable drawable);
  XftFont* xft_font_open_name(_Xconst char *name);
  void     draw_ucs2_string(XftDraw* draw, font_t *font, font_color *color,
                            const std::vector<uint16_t>& str, size_t x);

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

  // TODO: can this struct be replaced with just XftFont?
  struct font_t {
    struct glyph_t {
      FT_UInt id;
      XGlyphInfo info;
    };

    explicit font_t(const char* pattern, int offset = 0)
      : display(X::Instance().display)
      , xft_ft(XftFontOpenName(display, 0, pattern))
    {
      if (!xft_ft) {
        std::cerr << "Could not load font " << pattern << "\n";
        exit(EXIT_FAILURE);
      }

      descent = xft_ft->descent;
      height = xft_ft->ascent + descent;
      this->offset = offset;
    }
    ~font_t() {
      for (auto& [ch, glyph] : char_to_glyph) {
        XftFontUnloadGlyphs(display, xft_ft, &glyph.id, 1);
      }
      XftFontClose(display, xft_ft);
    }

    font_t(const font_t&) = delete;
    font_t(font_t&&) = delete;
    font_t& operator=(const font_t&) = delete;
    font_t& operator=(font_t&&) = delete;

    auto get_glyph(uint16_t ch) {
      auto itr = [this, ch] {
        std::shared_lock lock{_mu};
        return char_to_glyph.find(ch);
      }();

      if (itr == char_to_glyph.end()
          && XftCharExists(display, xft_ft, static_cast<FcChar32>(ch))) {
        std::unique_lock lock{_mu};
        return char_to_glyph.emplace(std::make_pair(ch, create_glyph(ch))).first;
      }
      return itr;
    }

    bool has_glyph(uint16_t ch) {
      return get_glyph(ch) != char_to_glyph.end();
    }

    // TODO: optimize for monospace fonts
    uint16_t char_width(uint16_t ch) {
      return get_glyph(ch)->second.info.xOff;
    }

    // TODO: can this be made const?
    // TODO: optimize for monospace fonts
    size_t string_size(const ucs2& str) {
      // TODO: cache in font_t instead of Fonts
      return std::accumulate(str.begin(), str.end(), 0,
          [this](size_t size, uint16_t ch) { return size + char_width(ch); });
    }

   private:
    glyph_t create_glyph(uint16_t ch) {
      XGlyphInfo glyph_info;
      FT_UInt glyph_id = XftCharIndex(display, xft_ft, static_cast<FcChar32>(ch));
      XftFontLoadGlyphs(display, xft_ft, FcFalse, &glyph_id, 1);
      XftGlyphExtents(display, xft_ft, &glyph_id, 1, &glyph_info);
      return {glyph_id, glyph_info};
    }

    Display *display;
    std::shared_mutex _mu;  // TODO: narrow down use
    std::unordered_map<uint16_t, glyph_t> char_to_glyph;

   public:
    XftFont *xft_ft;
    int descent { 0 };
    int height { 0 };
    int offset { 0 };
  };

 private:
  X();
  ~X();

  uint8_t get_depth() {
    return (visual == screen->root_visual) ? XCB_COPY_FROM_PARENT : 32;
  }

  Display            *display;
  xcb_connection_t   *connection;
  xcb_xrm_database_t *database;
  xcb_screen_t       *screen;

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
