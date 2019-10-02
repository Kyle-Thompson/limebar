#pragma once

#include <fontconfig/fontconfig.h>
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
  uint32_t generate_id() { return xcb_generate_id(connection); }
  void     flush() { xcb_flush(connection); }
  void     update_gc();

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
  void          set_event_queue_order(enum XEventQueueOwner owner);

  // temporary
  Display*            get_display()    { return display; }
  xcb_screen_t*       get_screen()     { return screen; }
  xcb_connection_t*   get_connection() { return connection; }
  xcb_xrm_database_t* get_database()   { return database; }

  // XFT functions
  bool     xft_char_exists(XftFont *pub, FcChar32 ucs4);
  FT_UInt  xft_char_index(XftFont *pub, FcChar32 ucs4);
  int      xft_char_width(uint16_t ch, XftFont *font);
  bool     xft_color_alloc_name(_Xconst Visual *visual, Colormap cmap,
                                _Xconst char *name, XftColor *result);
  void     xft_color_free(Visual *visual, Colormap cmap, XftColor *color);
  Visual*  xft_default_visual(int screen);
  XftDraw* xft_draw_create(Drawable drawable, Visual* visual,
                           Colormap colormap);
  void     xft_font_close(XftFont *xft);
  XftFont* xft_font_open_name(int screen, _Xconst char *name);
  
 private:
  X();
  ~X();

  Display            *display    { nullptr };
  xcb_screen_t       *screen     { nullptr };
  xcb_connection_t   *connection { nullptr };
  xcb_xrm_database_t *database   { nullptr };

  // char width lookuptable
  static constexpr size_t MAX_WIDTHS {1 << 16};
  wchar_t xft_char[MAX_WIDTHS] {0, };
  char    xft_width[MAX_WIDTHS] {0, };

  static X* instance;
};
