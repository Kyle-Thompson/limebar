#include "workspaces.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <bits/stdint-uintn.h>
#include <xcb/xcb.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#include "../x.h"

mod_workspaces::mod_workspaces()
    : conn(get_connection())
    , current_desktop(get_atom(conn, "_NET_CURRENT_DESKTOP"))
    , x(X::Instance()) {
  uint32_t values = XCB_EVENT_MASK_PROPERTY_CHANGE;
  xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
  xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK, &values);
  xcb_flush(conn);
}

void
mod_workspaces::extract(ModulePixmap *px) const {
  for (int i = 0; i < names.size(); ++i) {
    px->write(names[i] + " ", (i == cur_desktop));
  }
}

void
mod_workspaces::trigger() {
  while (true) {
    std::unique_ptr<xcb_generic_event_t, decltype(std::free) *> ev{
        xcb_wait_for_event(conn), std::free};
    if (!ev) continue;  // TODO: what is the right behavior here?
    if ((ev->response_type & 0x7F) == XCB_PROPERTY_NOTIFY &&
        reinterpret_cast<xcb_property_notify_event_t *>(ev.get())->atom ==
            current_desktop) {
      return;
    }
  }
}

void
mod_workspaces::update() {
  Window root = x.get_default_root_window();
  cur_desktop =
      x.get_property<uint64_t>(root, XA_CARDINAL, "_NET_CURRENT_DESKTOP")
          .value()
          .at(0);

  auto vec =
      x.get_property<char>(root, x.get_intern_atom(), "_NET_DESKTOP_NAMES")
          .value();
  char *chars = vec.data();

  names.clear();
  for (char *str = chars; str - chars < vec.size(); str += strlen(str) + 1) {
    names.emplace_back(str);
  }
}
