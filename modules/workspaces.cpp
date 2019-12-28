#include "workspaces.h"
#include "../x.h"

#include <bits/stdint-uintn.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <xcb/xcb.h>
#include <X11/Xatom.h>
#include <X11/X.h>

mod_workspaces::mod_workspaces()
  : conn(get_connection())
  , current_desktop(get_atom(conn, "_NET_CURRENT_DESKTOP"))
  , x(X::Instance())
{
  uint32_t values = XCB_EVENT_MASK_PROPERTY_CHANGE;
  xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
  xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK, &values);
  xcb_flush(conn);
}

void mod_workspaces::extract(ModulePixmap *px) const {
  for (int i = 0; i < names.size(); ++i) {
    if (i == cur_desktop) {
      px->write_with_accent(names[i] + " ");
    } else {
      px->write(names[i] + " ");
    }
  }
}

void mod_workspaces::trigger() {
  while (true) {
    std::unique_ptr<xcb_generic_event_t> ev(xcb_wait_for_event(conn));
    if (!ev) continue;  // TODO: what is the right behavior here?
    if ((ev->response_type & 0x7F) == XCB_PROPERTY_NOTIFY
        && reinterpret_cast<xcb_property_notify_event_t *>(ev.get())->atom
            == current_desktop) {
      return;
    }
  }
}

void mod_workspaces::update() {
  Window root = x.get_default_root_window();

  // TODO: fix memory leak
  auto *cur_desktop_ptr = x.get_property<uint64_t>(
      root, XA_CARDINAL, "_NET_CURRENT_DESKTOP", nullptr);
  cur_desktop = *cur_desktop_ptr;
  /* free(num_desktops_ptr); */

  uint64_t desktop_list_size { 0 };
  char *list = x.get_property<char>(root, x.get_intern_atom(),
                                    "_NET_DESKTOP_NAMES", &desktop_list_size);

  names.clear();
  for (char *str = list; str - list <= desktop_list_size; str += strlen(str) + 1) {
    names.emplace_back(str);
  }

  free(list);
}
