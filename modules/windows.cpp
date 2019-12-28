#include "windows.h"

#include <bits/stdint-uintn.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <X11/Xatom.h>
#include <X11/X.h>

mod_windows::mod_windows()
  : x(X::Instance())
{
  conn = xcb_connect(nullptr, nullptr);
  if (xcb_connection_has_error(conn)) {
    fprintf(stderr, "Cannot X connection for workspaces daemon.\n");
    exit(EXIT_FAILURE);
  }

  const char *window = "_NET_ACTIVE_WINDOW";
  xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn,
      xcb_intern_atom(conn, 0, static_cast<uint16_t>(strlen(window)), window), nullptr);
  active_window = reply ? reply->atom : XCB_NONE;
  free(reply);

  const char *desktop = "_NET_CURRENT_DESKTOP";
  reply = xcb_intern_atom_reply(conn,
      xcb_intern_atom(conn, 0, static_cast<uint16_t>(strlen(desktop)), desktop), nullptr);
  current_desktop = reply ? reply->atom : XCB_NONE;
  free(reply);

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
    auto *workspace = x.get_property<uint64_t>(
        w, XA_CARDINAL, "_NET_WM_DESKTOP", nullptr);
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
  for (xcb_generic_event_t *ev = nullptr; (ev = xcb_wait_for_event(conn)); free(ev)) {
    if ((ev->response_type & 0x7F) == XCB_PROPERTY_NOTIFY) {
      auto atom = reinterpret_cast<xcb_property_notify_event_t *>(ev)->atom;
      if (atom == active_window || atom == current_desktop) {
        free(ev);
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
