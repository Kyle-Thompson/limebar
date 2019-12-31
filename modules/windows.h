#pragma once

#include "module.h"

#include <xcb/xcb.h>
#include <X11/Xatom.h>

// TODO: make special window container

class mod_windows : public DynamicModule<mod_windows> {
  friend class DynamicModule<mod_windows>;

 public:
  mod_windows();
  ~mod_windows();

 private:
  template <typename DS>
  void extract(ModulePixmap<DS>* px) const;
  void trigger();
  void update();

  xcb_connection_t* conn;
  const xcb_atom_t current_desktop;
  const xcb_atom_t active_window;
  X& x;

  unsigned long  current_workspace;
  Window         current_window;
  std::vector<Window> windows;
};


template <typename DS>
void mod_windows::extract(ModulePixmap<DS> *px) const {
  // TODO: how to capture windows that don't work here? (e.g. steam)

  for (Window w : windows) {
    auto workspace = x.get_property<uint32_t>(w, XA_CARDINAL, "_NET_WM_DESKTOP")
        .value().at(0);
    std::string title = x.get_window_title(w);
    if (title.empty() || current_workspace != workspace) continue;

    // TODO: don't add a space after the last window
    px->write(title + " ", (w == current_window));
  }
}
