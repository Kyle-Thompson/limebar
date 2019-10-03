#pragma once

#include "module.h"

#include <xcb/xcb.h>

class mod_workspaces : public Module {
 public:
  mod_workspaces();
  ~mod_workspaces() {}

 private:
  void trigger();
  void update();

  xcb_atom_t current_desktop;
  xcb_connection_t* conn;
};
