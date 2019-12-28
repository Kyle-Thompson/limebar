#include "windows.h"

#include <bits/stdint-uintn.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <X11/Xatom.h>
#include <X11/X.h>

xcb_atom_t get_atom(xcb_connection_t *conn, const char *name) {
  std::unique_ptr<xcb_intern_atom_reply_t, decltype(std::free) *> reply {
      xcb_intern_atom_reply(conn,
          xcb_intern_atom(conn, 0, static_cast<uint16_t>(strlen(name)),
                          name),
          nullptr), std::free };
  return reply ? reply->atom : XCB_NONE;
}

mod_windows::mod_windows()
  : conn(xcb_connect(nullptr, nullptr))
  , current_desktop(get_atom(conn, "_NET_CURRENT_DESKTOP"))
  , active_window(get_atom(conn, "_NET_ACTIVE_WINDOW"))
  , x(X::Instance())
{
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

void mod_windows::extract(ModulePixmap *px) const {
  // TODO: how to capture windows that don't work here? (e.g. steam)

  for (Window w : windows) {
    auto *workspace = x.get_property<uint64_t>(w, XA_CARDINAL,
                                               "_NET_WM_DESKTOP", nullptr);
    std::string title = x.get_window_title(w);
    if (title.empty() || current_workspace != *workspace) continue;

    if (w == current_window) {
      px->write_with_accent(title);
    } else {
      px->write(title);
    }
    px->write(" ");  // TODO: don't add a space after the last window
  }
}

void mod_windows::trigger() {
  while (true) {
    std::unique_ptr<xcb_generic_event_t, decltype(std::free) *> ev {
        xcb_wait_for_event(conn), std::free };
    if (!ev) continue;  // TODO: what is the right behavior here?
    if ((ev->response_type & 0x7F) == XCB_PROPERTY_NOTIFY) {
      auto atom = reinterpret_cast<xcb_property_notify_event_t *>(ev.get())->atom;
      if (atom == active_window || atom == current_desktop) {
        return;
      }
    }
  }
}

void mod_windows::update() {
  // %{A:wmctrl -i -a 0x00c00003:}Firefox%{A}
  current_workspace = x.get_current_workspace();
  current_window = [this] {
    uint64_t size;
    auto* prop = x.get_property<Window>(x.get_default_root_window(), XA_WINDOW,
                                        "_NET_ACTIVE_WINDOW", &size);
    Window ret = *prop;
    free(prop);
    return ret;
  }();

  // TODO: must be a cleaner way to do this
  auto temp = x.get_windows();
  windows.clear();
  windows.insert(windows.begin(), temp.begin(), temp.end());
}
