#pragma once

#include <xcb/xcb.h>

#include <string>

#include "module.h"
#include "../x.h"

// TODO: make special window container

class mod_windows : public DynamicModule<mod_windows> {
  friend class DynamicModule<mod_windows>;

  struct window_t {
    xcb_window_t id;
    xcb_atom_t workspace;
    std::string title;
  };

 public:
  mod_windows();
  ~mod_windows();

 private:
  cppcoro::generator<segment_t> extract() const;
  void trigger();
  void update();

  xcb_connection_t* _conn;
  const xcb_atom_t _current_desktop;
  const xcb_atom_t _active_window;
  X11& _x;

  unsigned long _current_workspace{0};
  xcb_window_t _current_window{0};
  std::vector<window_t> _windows;
};
