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
  template <typename DS>
  void extract(ModulePixmap<DS>* px) const;
  void trigger();
  void update();

  xcb_connection_t* conn;
  xcb_atom_t current_desktop;
  X& x;

  uint32_t cur_desktop { 0 };
  std::vector<std::string> names;
};

template <typename DS>
void mod_workspaces::extract(ModulePixmap<DS> *px) const {
  for (int i = 0; i < names.size(); ++i) {
    px->write(names[i] + " ", (i == cur_desktop));
  }
}
