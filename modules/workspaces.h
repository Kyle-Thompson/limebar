#pragma once

#include <xcb/xcb.h>

#include <string>

#include "../pixmap.h"
#include "../x.h"
#include "module.h"

class mod_workspaces : public DynamicModule<mod_workspaces> {
  friend class DynamicModule<mod_workspaces>;

 public:
  mod_workspaces();

 private:
  cppcoro::generator<segment_t> extract() const;
  void trigger();
  void update();

  xcb_connection_t* conn;
  const xcb_atom_t current_desktop;
  X11& x;

  uint32_t cur_desktop{0};
  std::vector<std::string> names;
};
