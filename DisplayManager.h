#pragma once

#include <xcb/xcb.h>
#include <xcb/xcbext.h>
#include <xcb/randr.h>
#include <xcb/xcb_xrm.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xatom.h>

class DisplayManager {
 public:
  static DisplayManager* Instance();

  // TODO: return string
  char *get_property (Window win, Atom xa_prop_type,
      const char *prop_name, unsigned long *size);

  // TODO: return string
  char* get_window_title(Window win);

  Window* get_client_list(unsigned long *size);
  unsigned long get_current_workspace();
  Window get_default_root_window();
  Atom get_intern_atom() { return XInternAtom(display, "UTF8_STRING", 0); }
  XVisualInfo *get_visual_info(long vinfo_mask, XVisualInfo *vinfo_template, int *nitems_return) {
    return XGetVisualInfo(display, vinfo_mask, vinfo_template, nitems_return);
  };

  Display* get_display();

  xcb_connection_t *get_xcb_connection();
  void set_event_queue_order(enum XEventQueueOwner owner);

  // XFT functions
  bool xft_char_exists(XftFont *pub, FcChar32 ucs4) {
    return XftCharExists(display, pub, ucs4);
  }
  FT_UInt xft_char_index(XftFont *pub, FcChar32 ucs4) {
    return XftCharIndex(display, pub, ucs4);
  }
  /* int      xft_char_width(uint16_t ch, XftFont *xft_ft); */
  bool     xft_color_alloc_name(_Xconst Visual *visual, Colormap cmap, _Xconst char *name, XftColor *result);
  void     xft_color_free(Visual *visual, Colormap cmap, XftColor *color);
  Visual  *xft_default_visual(int screen);
  XftDraw *xft_draw_create(Drawable drawable, Visual* visual, Colormap colormap);
  XftFont *xft_font_open_name(int screen, _Xconst char *name);
  

 private:
  DisplayManager();
  ~DisplayManager();

  Display* display;
  static DisplayManager* instance;
};
