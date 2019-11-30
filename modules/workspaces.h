#pragma once

#include "dynamic_module.h"

#include <string>
#include <xcb/xcb.h>

class mod_workspaces : public DynamicModule<mod_workspaces> {
 public:
  mod_workspaces(const BarWindow& win);
  ~mod_workspaces() {}

  void trigger();
  void update();

  constexpr static size_t MAX_AREAS = 10;

 private:
  xcb_atom_t current_desktop;
  xcb_connection_t* conn;
};
