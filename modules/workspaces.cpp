#include "workspaces.h"
#include "../x.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <xcb/xcb.h>
#include <X11/Xatom.h>
#include <X11/X.h>

mod_workspaces::mod_workspaces()
{
  conn = xcb_connect(nullptr, nullptr);
  if (xcb_connection_has_error(conn)) {
    fprintf(stderr, "Cannot create X connection for workspaces module.\n");
    exit(EXIT_FAILURE);
  }

  const char *desktop = "_NET_CURRENT_DESKTOP";
  xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn,
      xcb_intern_atom(conn, 0, static_cast<uint16_t>(strlen(desktop)), desktop),
      nullptr);
  current_desktop = reply ? reply->atom : XCB_NONE;
  free(reply);

  uint32_t values = XCB_EVENT_MASK_PROPERTY_CHANGE;
  xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
  xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK, &values);
  xcb_flush(conn);
}

void mod_workspaces::trigger()
{
  // TODO: this doesn't work if switching an empty workspace to an empty
  // workspace.
  for (xcb_generic_event_t *ev = nullptr; (ev = xcb_wait_for_event(conn));
      free(ev)) {
    if ((ev->response_type & 0x7F) == XCB_PROPERTY_NOTIFY
        && reinterpret_cast<xcb_property_notify_event_t *>(ev)->atom
            == current_desktop) {
      free(ev);
      return;
    }
  }
}

std::string mod_workspaces::update()
{
  unsigned long desktop_list_size = 0;
  Window root = X::Instance()->get_default_root_window();

  unsigned long *num_desktops =
    (unsigned long *)X::Instance()->get_property(root,
        XA_CARDINAL, "_NET_NUMBER_OF_DESKTOPS", nullptr);
  unsigned long *cur_desktop =
    (unsigned long *)X::Instance()->get_property(root,
        XA_CARDINAL, "_NET_CURRENT_DESKTOP", nullptr);
  char *list = X::Instance()->get_property(root,
      X::Instance()->get_intern_atom(),
      "_NET_DESKTOP_NAMES", &desktop_list_size);

  /* prepare the array of desktop names */
  char **names = (char **) malloc(*num_desktops * sizeof(char *));
  int id = 0;
  names[id++] = list;
  for (int i = 0; i < desktop_list_size; i++) {
    if (list[i] == '\0') {
      if (id >= *num_desktops) {
        break;
      }
      names[id++] = list + i + 1;
    }
  }

  // %{A:wmctrl -s 1 && refbar workspaces windows:}2%{A}
  std::stringstream ss;
  for (int i = 0; i < *num_desktops; ++i) {
    if (i == *cur_desktop) ss << "%{F#257fad}";
    ss << names[i] << " ";
    if (i == *cur_desktop) ss << "%{F#7ea2b4}";
  }

  free(names);
  free(num_desktops);
  free(cur_desktop);
  free(list);
  return ss.str();
}
