#pragma once

#include <xcb/xcb.h>

#include <string>

#include "../config.h"
#include "../types.h"
#include "module.h"

// TODO: make special window container

class mod_windows : public DynamicModule<mod_windows> {
  friend class DynamicModule<mod_windows>;

 public:
  mod_windows();
  ~mod_windows();

  bool has_work();
  void do_work();

 private:
  xcb_connection_t* _conn;
  const xcb_atom_t _current_desktop_atom;
  const xcb_atom_t _active_window_atom;
  DS& _ds;

  std::vector<segment_t> _segments;
};
