#pragma once

#include <X11/Xatom.h>
#include <xcb/xcb.h>

#include <string>

#include "module.h"

// TODO: make special window container

class mod_windows : public DynamicModule<mod_windows> {
  friend class DynamicModule<mod_windows>;

  struct window_t {
    Window id;
    xcb_atom_t workspace;
    std::string title;
  };

 public:
  mod_windows();
  ~mod_windows();

 private:
  void extract(ModulePixmap* px) const;
  void trigger();
  void update();

  xcb_connection_t* _conn;
  const xcb_atom_t _current_desktop;
  const xcb_atom_t _active_window;
  X& _x;

  unsigned long _current_workspace { 0 };
  Window _current_window { 0 };
  std::vector<window_t> _windows;
};
