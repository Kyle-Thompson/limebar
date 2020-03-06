#include "windows.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <bits/stdint-uintn.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>

#include "../x.h"


mod_windows::mod_windows()
    : conn(xcb_connect(nullptr, nullptr))
    , current_desktop(get_atom(conn, "_NET_CURRENT_DESKTOP"))
    , active_window(get_atom(conn, "_NET_ACTIVE_WINDOW"))
    , x(X::Instance()) {
  if (xcb_connection_has_error(conn)) {
    std::cerr << "Cannot X connection for workspaces daemon.\n";
    exit(EXIT_FAILURE);
  }

  uint32_t values = XCB_EVENT_MASK_PROPERTY_CHANGE;
  xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
  xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK, &values);
  xcb_flush(conn);
}

mod_windows::~mod_windows() {
  xcb_disconnect(conn);
}

void
mod_windows::extract(ModulePixmap *px) const {
  // TODO: how to capture windows that don't work here? (e.g. steam)

  for (Window w : windows) {
    auto workspace = x.get_property<uint32_t>(w, XA_CARDINAL, "_NET_WM_DESKTOP")
                         .value()
                         .at(0);
    std::string title = x.get_window_title(w);
    if (title.empty() || current_workspace != workspace) continue;

    // TODO: don't add a space after the last window
    px->write(title + " ", (w == current_window));
  }
}

void
mod_windows::trigger() {
  while (true) {
    std::unique_ptr<xcb_generic_event_t, decltype(std::free) *> ev{
        xcb_wait_for_event(conn), std::free};
    if (!ev) continue;  // TODO: what is the right behavior here?
    if ((ev->response_type & 0x7F) == XCB_PROPERTY_NOTIFY) {
      auto atom =
          reinterpret_cast<xcb_property_notify_event_t *>(ev.get())->atom;
      if (atom == active_window || atom == current_desktop) {
        return;
      }
    }
  }
}

void
mod_windows::update() {
  current_workspace = x.get_current_workspace();
  current_window = x.get_property<Window>(x.get_default_root_window(),
                                          XA_WINDOW, "_NET_ACTIVE_WINDOW")
                       .value()
                       .at(0);

  // TODO: must be a cleaner way to do this
  auto temp = x.get_windows();
  windows.clear();
  windows.insert(windows.begin(), temp.begin(), temp.end());
}
