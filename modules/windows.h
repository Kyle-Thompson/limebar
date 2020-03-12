#pragma once

#include <X11/Xatom.h>
#include <xcb/xcb.h>

#include "module.h"

// TODO: make special window container

class mod_windows : public DynamicModule<mod_windows> {
  friend class DynamicModule<mod_windows>;

 public:
  mod_windows();
  ~mod_windows();

 private:
  void extract(ModulePixmap* px) const;
  void trigger();
  void update();

  xcb_connection_t* conn;
  const xcb_atom_t current_desktop;
  const xcb_atom_t active_window;
  X& x;

  unsigned long _current_workspace { 0 };
  Window _current_window { 0 };
  std::vector<Window> _windows;
};
