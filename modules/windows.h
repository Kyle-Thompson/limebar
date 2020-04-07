#pragma once

#include <xcb/xcb.h>

#include <string>

#include "module.h"
#include "../x.h"

// TODO: make special window container

class mod_windows : public DynamicModule<mod_windows> {
  friend class DynamicModule<mod_windows>;

 public:
  mod_windows();
  ~mod_windows();

 private:
  auto extract() const -> cppcoro::generator<segment_t>;
  void trigger();
  void update();

  xcb_connection_t* _conn;
  const xcb_atom_t _current_desktop_atom;
  const xcb_atom_t _active_window_atom;
  X11& _x;

  std::vector<segment_t> _segments;
};
