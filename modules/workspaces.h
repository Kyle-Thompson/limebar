#pragma once

#include <xcb/xcb.h>

#include "../config.h"
#include "../types.h"
#include "module.h"

class mod_workspaces : public DynamicModule<mod_workspaces> {
  friend class DynamicModule<mod_workspaces>;

 public:
  mod_workspaces();

  bool has_work();
  void do_work();

 private:
  xcb_connection_t* _conn;
  const xcb_atom_t _current_desktop;
  DS& _ds;

  std::vector<segment_t> _segments;
};
