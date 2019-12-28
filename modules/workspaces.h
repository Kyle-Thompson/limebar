#pragma once

#include "module.h"
#include "../pixmap.h"
#include "../x.h"

#include <string>
#include <xcb/xcb.h>

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

  unsigned long cur_desktop { 0 };
  std::vector<std::string> names;
};
