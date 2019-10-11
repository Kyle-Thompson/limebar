#pragma once

#include <string>
#include <xcb/xcb.h>

class mod_workspaces {
 public:
  mod_workspaces();
  ~mod_workspaces() {}

  void trigger();
  std::string update();

  constexpr static size_t MAX_AREAS = 10;

 private:
  xcb_atom_t current_desktop;
  xcb_connection_t* conn;
};
