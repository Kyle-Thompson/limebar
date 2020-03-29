#include <X11/Xlib.h>

#include <iostream>
#include <tuple>

#include "bars.h"
#include "color.h"
#include "config.h"
#include "font.h"
#include "modules/clock.h"
#include "modules/fill.h"
#include "modules/module.h"
#include "modules/windows.h"
#include "modules/workspaces.h"

int
main() {
  DS::init();

  ModuleContainer<mod_fill> space(" ");
  ModuleContainer<mod_workspaces> workspaces;
  ModuleContainer<mod_fill> sep("| ");
  ModuleContainer<mod_windows> windows;
  ModuleContainer<mod_clock> clock;

  auto left_section{make_section(space, workspaces, sep, windows)};
  auto middle_section{make_section(clock)};
  auto right_section{make_section()};

  auto& ds = DS::Instance();

  rgba_t bgc = rgba_t::parse(ds.get_string_resource("background").c_str());
  rgba_t fgc = rgba_t::parse(ds.get_string_resource("foreground").c_str());
  rgba_t acc = rgba_t::parse(ds.get_string_resource("color4").c_str());
  DS::font_t ft(ds.get_string_resource("font").c_str());

  Bar left_bar({.origin_x = 0, .origin_y = 0, .width = 1920, .height = 20},
               {.background{bgc}, .foreground{fgc}, .fg_accent{acc}}, {&ft},
               left_section, middle_section, right_section);

  Bar middle_bar({.origin_x = 1920, .origin_y = 0, .width = 1920, .height = 20},
                 {.background{bgc}, .foreground{fgc}, .fg_accent{acc}}, {&ft},
                 left_section, middle_section, right_section);

  Bar right_bar({.origin_x = 3840, .origin_y = 0, .width = 1920, .height = 20},
                {.background{bgc}, .foreground{fgc}, .fg_accent{acc}}, {&ft},
                left_section, middle_section, right_section);

  BarContainer bars(left_bar, middle_bar, right_bar);
}
