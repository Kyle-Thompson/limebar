#pragma once

#include "module.h"
#include "../pixmap.h"

#include <string>
#include <xcb/xcb.h>

class mod_workspaces : public DynamicModule<mod_workspaces> {
 public:
  mod_workspaces();

  void get(ModulePixmap& px);

  constexpr static size_t MAX_AREAS = 10;

  friend class DynamicModule<mod_workspaces>;
 private:
  void trigger();
  void update();

  xcb_atom_t current_desktop;
  xcb_connection_t* conn;

  unsigned long *num_desktops { nullptr };
  unsigned long *cur_desktop  { nullptr };
  char **names                { nullptr };
};
