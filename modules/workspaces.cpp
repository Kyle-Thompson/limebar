// TODO: some of the Xlib includes have name collisions here. Remove when
// porting to modules.
// clang-format off
#include <range/v3/view/enumerate.hpp>
#include <range/v3/range/conversion.hpp>
// clang-format on

#include "workspaces.h"


mod_workspaces::mod_workspaces()
    : _conn(get_connection())
    , _current_desktop(get_atom(_conn, "_NET_CURRENT_DESKTOP"))
    , _ds(DS::Instance()) {
  uint32_t values = XCB_EVENT_MASK_PROPERTY_CHANGE;
  xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(_conn)).data;
  xcb_change_window_attributes(_conn, screen->root, XCB_CW_EVENT_MASK, &values);
  xcb_flush(_conn);
}

bool
mod_workspaces::has_work() {
  while (true) {
    std::unique_ptr<xcb_generic_event_t, decltype(std::free) *> ev{
        xcb_poll_for_event(_conn), std::free};
    if (!ev) {
      return false;
    }
    // TODO: can we filter in the display server to only return these values
    // in the first place so we don't have to check every time?
    if ((ev->response_type & 0x7F) == XCB_PROPERTY_NOTIFY &&
        reinterpret_cast<xcb_property_notify_event_t *>(ev.get())->atom ==
            _current_desktop) {
      return true;
    }
  }
}

void
mod_workspaces::do_work() {
  uint32_t cur_desktop = _ds.get_current_workspace();

  _segments.clear();
  for (auto names = _ds.get_workspace_names();
       auto &&[i, name] : names | ranges::views::enumerate) {
    _segments.push_back(
        {.segments{{.str{name + ' '},
                    .color = (i == cur_desktop ? ACCENT_COLOR : NORMAL_COLOR)}},
         .action = [desk = i, this](uint8_t button) {
           if (button == 1) {
             _ds.switch_desktop(desk);
           }
         }});
  }
}
