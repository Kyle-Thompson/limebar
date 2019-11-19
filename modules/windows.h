#pragma once

#include "module.h"

#include "../x.h"

#include <string>
#include <xcb/xcb.h>

class mod_windows : public Module<mod_windows> {
 public:
  mod_windows(const BarWindow& win);
  ~mod_windows();

  void trigger();
  void update();

  constexpr static size_t MAX_AREAS = 20;

 private:
  xcb_connection_t* conn;
  xcb_atom_t current_desktop;
  xcb_atom_t active_window;
};
