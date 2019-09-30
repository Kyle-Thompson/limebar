#pragma once

#include "module.h"
#include "../DisplayManager.h"

#include <xcb/xcb.h>

class mod_windows : public module {
 public:
  mod_windows();
  ~mod_windows();

 private:
  void trigger();
  void update();

  xcb_connection_t* conn;
  xcb_atom_t current_desktop;
  xcb_atom_t active_window;
};

