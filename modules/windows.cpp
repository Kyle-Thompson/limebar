#include "windows.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>

#include "../x.h"


mod_windows::mod_windows()
    : _conn(get_connection())
    , _current_desktop_atom(get_atom(_conn, "_NET_CURRENT_DESKTOP"))
    , _active_window_atom(get_atom(_conn, "_NET_ACTIVE_WINDOW"))
    , _x(X11::Instance()) {
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

// TODO: const segment_t&
cppcoro::generator<segment_t>
mod_windows::extract() const {
  for (segment_t seg : _segments) {
    co_yield seg;
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
      if (atom == _active_window_atom || atom == _current_desktop_atom) {
        return;
      }
    }
  }
}

void
mod_windows::update() {
  uint32_t current_workspace = _x.get_current_workspace();
  xcb_window_t current_window = _x.get_active_window();

  _segments.clear();
  for (xcb_window_t window : _x.get_windows()) {
    std::string title = _x.get_window_title(window);
    if (title.empty()) {
      continue;
    }

    auto workspace = _x.get_workspace_of_window(window);
    if (!workspace.has_value() || workspace != current_workspace) {
      continue;
    }

    _segments.push_back(
        {.segments{{.str = title + ' ',
                    .color = (window == current_window ? ACCENT_COLOR
                                                       : NORMAL_COLOR)}},
         .action = [this, window](uint8_t button) {
           if (button == 1) {
             _x.activate_window(window);
           }
         }});
  }
}
