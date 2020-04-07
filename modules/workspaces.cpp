// TODO: some of the Xlib includes have name collisions here. Remove when
// porting to modules.
// clang-format off
#include <range/v3/view/enumerate.hpp>
#include <range/v3/range/conversion.hpp>
// clang-format on

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
    : _conn(get_connection())
    , _current_desktop(get_atom(_conn, "_NET_CURRENT_DESKTOP"))
    , _x(X11::Instance()) {
  uint32_t values = XCB_EVENT_MASK_PROPERTY_CHANGE;
  xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(_conn)).data;
  xcb_change_window_attributes(_conn, screen->root, XCB_CW_EVENT_MASK, &values);
  xcb_flush(_conn);
}

void
mod_workspaces::trigger() {
  while (true) {
    std::unique_ptr<xcb_generic_event_t, decltype(std::free) *> ev{
        xcb_wait_for_event(_conn), std::free};
    if (ev && (ev->response_type & 0x7F) == XCB_PROPERTY_NOTIFY &&
        reinterpret_cast<xcb_property_notify_event_t *>(ev.get())->atom ==
            _current_desktop) {
      return;
    }
  }
}

void
mod_workspaces::update() {
  uint32_t cur_desktop = _x.get_current_workspace();

  _segments.clear();
  for (auto names = _x.get_workspace_names();
       const auto &[i, name] : names | ranges::views::enumerate) {
    _segments.push_back(
        {.segments{{.str = name + ' ',
                    .color = (i == cur_desktop ? ACCENT_COLOR : NORMAL_COLOR)}},
         .action = [desk = i, this](uint8_t button) {
           if (button == 1) {
             _x.switch_desktop(desk);
           }
         }});
  }
}
