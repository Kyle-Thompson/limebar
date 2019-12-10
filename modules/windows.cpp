#include "windows.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <X11/Xatom.h>
#include <X11/X.h>

mod_windows::mod_windows(const BarWindow& win)
  : DynamicModule(win)
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

  uint32_t values = XCB_EVENT_MASK_PROPERTY_CHANGE;
  xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
  xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK, &values);
  xcb_flush(conn);

  const char *desktop = "_NET_CURRENT_DESKTOP";
  reply = xcb_intern_atom_reply(conn,
      xcb_intern_atom(conn, 0, static_cast<uint16_t>(strlen(desktop)), desktop), nullptr);
  current_desktop = reply ? reply->atom : XCB_NONE;
  free(reply);

  screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
  xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK, &values);
  xcb_flush(conn);
}

mod_windows::~mod_windows() {
  xcb_disconnect(conn);
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
  unsigned long client_list_size;
  unsigned long current_workspace = X::Instance().get_current_workspace();

  const Window current_window = [] {
    unsigned long size;
    char* prop = X::Instance().get_property<char>(X::Instance()
        .get_default_root_window(), XA_WINDOW, "_NET_ACTIVE_WINDOW", &size);
    Window ret = *((Window*)prop);
    free(prop);
    return ret;
  }();

  // TODO: how to capture windows that don't work here? (e.g. steam)
  Window* windows = X::Instance().get_client_list(&client_list_size);
  for (unsigned long i = 0; i < client_list_size / sizeof(Window); ++i) {
    unsigned long *workspace = X::Instance().get_property<unsigned long>(
        windows[i], XA_CARDINAL, "_NET_WM_DESKTOP", nullptr);
    std::string title = X::Instance().get_window_title(windows[i]);  // TODO fix leak
    if (title.empty() || current_workspace != *workspace) continue;
    if (windows[i] == current_window) {
      _pixmap.write_with_accent(title.substr(title.find_last_of(' ') + 1) + ' ');
    } else {
      _pixmap.write(title.substr(title.find_last_of(' ') + 1) + ' ');
    }
  }
  free(windows);
}
