#pragma once

#include "module.h"

#include <xcb/xcb.h>

class mod_workspaces : public module {
 public:
  mod_workspaces();
  ~mod_workspaces() {}

 private:
  void trigger();
  void update();

  xcb_connection_t* conn;
  xcb_atom_t current_desktop;
};
