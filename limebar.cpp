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
  auto rdb = ds.create_resource_database();

  rgba_t bgc = rgba_t::parse(rdb.get<std::string>("background").c_str());
  rgba_t fgc = rgba_t::parse(rdb.get<std::string>("foreground").c_str());
  DS::font_color_t ft_fg = ds.create_font_color(fgc);
  rgba_t acc = rgba_t::parse(rdb.get<std::string>("color4").c_str());
  DS::font_color_t ft_acc = ds.create_font_color(acc);

  DS::font_t ft = ds.create_font(rdb.get<std::string>("font").c_str());

  Bar left_bar({.x = 0, .y = 0, .width = 1920, .height = 20},
               {.background{bgc}, .foreground{ft_fg}, .fg_accent{ft_acc}},
               {&ft}, left_section, middle_section, right_section);

  Bar middle_bar({.x = 1920, .y = 0, .width = 1920, .height = 20},
                 {.background{bgc}, .foreground{ft_fg}, .fg_accent{ft_acc}},
                 {&ft}, left_section, middle_section, right_section);

  Bar right_bar({.x = 3840, .y = 0, .width = 1920, .height = 20},
                {.background{bgc}, .foreground{ft_fg}, .fg_accent{ft_acc}},
                {&ft}, left_section, middle_section, right_section);

  BarContainer bars(left_bar, middle_bar, right_bar);
}
