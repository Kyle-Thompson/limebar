#include "workspaces.h"

#include <xcb/xcb.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#include "../types.h"
#include "../x.h"

mod_workspaces::mod_workspaces()
    : conn(get_connection())
    , current_desktop(get_atom(conn, "_NET_CURRENT_DESKTOP"))
    , x(X11::Instance()) {
  uint32_t values = XCB_EVENT_MASK_PROPERTY_CHANGE;
  xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
  xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK, &values);
  xcb_flush(conn);
}

cppcoro::generator<segment_t>
mod_workspaces::extract() const {
  for (int i = 0; i < names.size(); ++i) {
    text_segment_t text_seg{
        .str = names[i] + ' ',
        .color = (i == cur_desktop ? ACCENT_COLOR : NORMAL_COLOR)};

    segment_t seg{.segments{text_seg},
                  .action = [this, i](uint8_t button) { x.switch_desktop(i); }};
    co_yield seg;
  }
}

void
mod_workspaces::trigger() {
  while (true) {
    std::unique_ptr<xcb_generic_event_t, decltype(std::free) *> ev{
        xcb_wait_for_event(conn), std::free};
    if (ev && (ev->response_type & 0x7F) == XCB_PROPERTY_NOTIFY &&
        reinterpret_cast<xcb_property_notify_event_t *>(ev.get())->atom ==
            current_desktop) {
      return;
    }
  }
}

void
mod_workspaces::update() {
  cur_desktop = x.get_current_workspace();
  names = x.get_workspace_names();
}
