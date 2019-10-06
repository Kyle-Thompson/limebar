#pragma once

#include <string>
#include <xcb/xcb.h>

class mod_workspaces {
 public:
  mod_workspaces();
  ~mod_workspaces() {}

  void trigger();
  std::string update();

 private:
  xcb_atom_t current_desktop;
  xcb_connection_t* conn;
};
