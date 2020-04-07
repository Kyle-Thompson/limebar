#pragma once

#include <xcb/xcb.h>

#include <string>

#include "module.h"
#include "../x.h"

class mod_workspaces : public DynamicModule<mod_workspaces> {
  friend class DynamicModule<mod_workspaces>;

 public:
  mod_workspaces();

 private:
  auto extract() const -> cppcoro::generator<segment_t>;
  void trigger();
  void update();

  xcb_connection_t* _conn;
  const xcb_atom_t _current_desktop;
  X11& _x;

  std::vector<segment_t> _segments;
};
