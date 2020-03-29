#include "windows.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <bits/stdint-uintn.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>

#include "../x.h"


// TODO it's probably an issue to have two different x connections. (conn, _x)

mod_windows::mod_windows()
    : _conn(xcb_connect(nullptr, nullptr))
    , _current_desktop(get_atom(_conn, "_NET_CURRENT_DESKTOP"))
    , _active_window(get_atom(_conn, "_NET_ACTIVE_WINDOW"))
    , _x(X::Instance()) {
  if (xcb_connection_has_error(_conn)) {
    std::cerr << "Cannot X connection for workspaces daemon.\n";
    exit(EXIT_FAILURE);
  }

  uint32_t values = XCB_EVENT_MASK_PROPERTY_CHANGE;
  xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(_conn)).data;
  xcb_change_window_attributes(_conn, screen->root, XCB_CW_EVENT_MASK, &values);
  xcb_flush(_conn);
}

mod_windows::~mod_windows() {
  xcb_disconnect(_conn);
}

void
mod_windows::extract(ModulePixmap *px) const {
  for (const window_t& window : _windows) {
    if (window.workspace != _current_workspace) {
      continue;
    }

    // TODO: don't add a space after the last window
    px->write(window.title + " ", (window.id == _current_window));
  }
}

void
mod_windows::trigger() {
  while (true) {
    std::unique_ptr<xcb_generic_event_t, decltype(std::free) *> ev{
        xcb_wait_for_event(_conn), std::free};
    if (ev && (ev->response_type & 0x7F) == XCB_PROPERTY_NOTIFY) {
      auto atom =
          reinterpret_cast<xcb_property_notify_event_t *>(ev.get())->atom;
      if (atom == _active_window || atom == _current_desktop) {
        return;
      }
    }
  }
}

void
mod_windows::update() {
  _current_workspace = _x.get_current_workspace();
  _current_window = _x.get_property<Window>(_x.get_default_root_window(),
                                            XA_WINDOW, "_NET_ACTIVE_WINDOW")
                        .value()
                        .at(0);

  _windows.clear();
  for (auto windows = _x.get_windows(); auto window : windows) {
    std::string title = _x.get_window_title(window);
    if (title.empty()) {
      continue;
    }

    auto workspace =
        _x.get_property<uint32_t>(window, XA_CARDINAL, "_NET_WM_DESKTOP")
            .value()
            .at(0);

    _windows.push_back({
        .id = window,
        .workspace = workspace,
        .title = std::move(title)
    });

  }
}
