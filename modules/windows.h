#pragma once

#include "module.h"

#include <xcb/xcb.h>

// TODO: make special window container

class mod_windows : public DynamicModule<mod_windows> {
 public:
  mod_windows();
  ~mod_windows();

  void get(ModulePixmap& px) const;

  constexpr static size_t MAX_AREAS = 20;

  friend class DynamicModule<mod_windows>;
 private:
  void trigger();
  void update();

  xcb_connection_t* conn;
  xcb_atom_t current_desktop;
  xcb_atom_t active_window;

  unsigned long  current_workspace;
  Window         current_window;
  std::vector<Window> windows;
};
