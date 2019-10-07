#pragma once

#include "color.h"
#include "enums.h"

#include <fontconfig/fontconfig.h>
#include <mutex>
#include <xcb/xcb.h>
#include <xcb/xcb_xrm.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>

class X {
 public:
  static X* Instance();

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
                     int16_t dst_x, uint16_t width);

  // TODO: label
  char*         get_property(Window win, Atom xa_prop_type,
                             const char *prop_name, unsigned long *size);
  char*         get_window_title(Window win);
  Window*       get_client_list(unsigned long *size);
  unsigned long get_current_workspace();
  Window        get_default_root_window();
  Atom          get_intern_atom();
  XVisualInfo*  get_visual_info(long vinfo_mask, XVisualInfo *vinfo_template,
                                int *nitems_return);
  void          get_randr_monitors();

  void set_ewmh_atoms();
  void set_event_queue_order(enum XEventQueueOwner owner);
  auto get_gc() { return gc; }

  // temporary
  auto get_display()    { return display; }
  auto get_screen()     { return screen; }
  auto get_connection() { return connection; }
  auto get_database()   { return database; }
  auto get_selfg()      { return sel_fg; }
  auto get_selfg_ptr()  { return &sel_fg; }

  xcb_visualid_t get_visual();

  // XFT functions
  bool     xft_char_exists(XftFont *pub, FcChar32 ucs4);
  FT_UInt  xft_char_index(XftFont *pub, FcChar32 ucs4);
  int      xft_char_width(uint16_t ch, XftFont *font);
  bool     xft_color_alloc_name(_Xconst char *name);
  void     xft_color_free();
  Visual*  xft_default_visual(int screen);
  XftDraw* xft_draw_create(Drawable drawable);
  void     xft_font_close(XftFont *xft);
  XftFont* xft_font_open_name(_Xconst char *name);

  // TODO: make these private
  rgba_t fgc, bgc, ugc;
  XftDraw *xft_draw { nullptr };
  
 private:
  X();
  ~X();
  void init();

  Display            *display    { nullptr };
  xcb_screen_t       *screen     { nullptr };
  xcb_connection_t   *connection { nullptr };
  xcb_xrm_database_t *database   { nullptr };

  // char width lookuptable
  static constexpr size_t MAX_WIDTHS {1 << 16};
  wchar_t xft_char[MAX_WIDTHS] {0, };
  char    xft_width[MAX_WIDTHS] {0, };

  xcb_gcontext_t gc[GC_MAX];
  XftColor sel_fg;
  xcb_colormap_t colormap;

  Visual *visual_ptr;
  xcb_visualid_t visual;

  // static std::mutex _init_mutex;
  static X* instance;
};
