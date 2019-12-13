#pragma once

#include <xcb/xcb.h>

#include "module.h"

// TODO: make special window container

class mod_windows : public DynamicModule<mod_windows> {
  friend class DynamicModule<mod_windows>;

 public:
  mod_windows();
  ~mod_windows();

 private:
  void extract(ModulePixmap& px) const;
  void trigger();
  void update();

  xcb_connection_t* conn;
  xcb_atom_t current_desktop;
  xcb_atom_t active_window;

  unsigned long current_workspace;
  Window current_window;
  std::vector<Window> windows;
};
