#pragma once

#include "../x.h"

#include <string>
#include <xcb/xcb.h>

class mod_windows {
 public:
  mod_windows();
  ~mod_windows();

  void trigger();
  std::string update();

 private:
  xcb_connection_t* conn;
  xcb_atom_t current_desktop;
  xcb_atom_t active_window;
};
