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
  Display* get_display();

 private:
  DisplayManager();
  ~DisplayManager();

  Display* display;
  static DisplayManager* instance;
};
