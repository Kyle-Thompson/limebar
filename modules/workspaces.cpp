#include "workspaces.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <xcb/xcb.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "../x.h"

mod_workspaces::mod_workspaces() {
  conn = xcb_connect(nullptr, nullptr);
  if (xcb_connection_has_error(conn)) {
    fprintf(stderr, "Cannot create X connection for workspaces module.\n");
    exit(EXIT_FAILURE);
  }

  const char *desktop = "_NET_CURRENT_DESKTOP";
  xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(
      conn,
      xcb_intern_atom(conn, 0, static_cast<uint16_t>(strlen(desktop)), desktop),
      nullptr);
  current_desktop = reply ? reply->atom : XCB_NONE;
  free(reply);

  uint32_t values = XCB_EVENT_MASK_PROPERTY_CHANGE;
  xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
  xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK, &values);
  xcb_flush(conn);
}

void
mod_workspaces::extract(ModulePixmap &px) const {
  for (int i = 0; i < names.size(); ++i) {
    if (i == cur_desktop) {
      px.write_with_accent(names[i] + " ");
    } else {
      px.write(names[i] + " ");
    }
  }
}

void
mod_workspaces::trigger() {
  // TODO: this doesn't work if switching an empty workspace to an empty
  // workspace.
  for (xcb_generic_event_t *ev = nullptr; (ev = xcb_wait_for_event(conn));
       free(ev)) {
    if ((ev->response_type & 0x7F) == XCB_PROPERTY_NOTIFY &&
        reinterpret_cast<xcb_property_notify_event_t *>(ev)->atom ==
            current_desktop) {
      free(ev);
      return;
    }
  }
}

void
mod_workspaces::update() {
  Window root = X::Instance().get_default_root_window();

  // TODO: fix memory leak
  unsigned long *cur_desktop_ptr = X::Instance().get_property<unsigned long>(
      root, XA_CARDINAL, "_NET_CURRENT_DESKTOP", nullptr);
  cur_desktop = *cur_desktop_ptr;
  /* free(num_desktops_ptr); */

  unsigned long desktop_list_size{0};
  char *list = X::Instance().get_property<char>(
      root, X::Instance().get_intern_atom(), "_NET_DESKTOP_NAMES",
      &desktop_list_size);

  char *str = list;
  names.clear();
  for (int offset = 0; offset <= desktop_list_size; offset += strlen(str) + 1) {
    names.push_back(str + offset);
  }

  free(list);
}
