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
  void extract(ModulePixmap* px) const;
  void trigger();
  void update();

  xcb_connection_t* conn;
  xcb_atom_t current_desktop;
  X& x;

  uint32_t cur_desktop{0};
  std::vector<std::string> names;
};
