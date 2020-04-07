#include <chrono>
#include <thread>
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
#include "task.h"


int
main() {
  mod_fill space(" ");
  mod_workspaces workspaces;
  mod_fill sep("| ");
  mod_windows windows;
  mod_clock clock;

  auto left_section{section_wrapper(space, workspaces, sep, windows)};
  auto middle_section{section_wrapper(clock)};
  auto right_section{section_wrapper()};

  auto& ds = DS::Instance();
  auto rdb = ds.create_resource_database();

  rgba_t bgc = rgba_t::parse(rdb.get<std::string>("background").c_str());
  rgba_t fgc = rgba_t::parse(rdb.get<std::string>("foreground").c_str());
  DS::font_color_t ft_fg = ds.create_font_color(fgc);
  rgba_t acc = rgba_t::parse(rdb.get<std::string>("color4").c_str());
  DS::font_color_t ft_acc = ds.create_font_color(acc);

  DS::font_t ft = ds.create_font(rdb.get<std::string>("font").c_str());

  Bar<decltype(left_section), decltype(middle_section), decltype(right_section)>
      left_bar({.x = 0, .y = 0, .width = 1920, .height = 20},
               {.background{bgc}, .foreground{ft_fg}, .fg_accent{ft_acc}},
               {&ft}, left_section, middle_section, right_section);

  Bar<decltype(left_section), decltype(middle_section), decltype(right_section)>
      middle_bar({.x = 1920, .y = 0, .width = 1920, .height = 20},
                 {.background{bgc}, .foreground{ft_fg}, .fg_accent{ft_acc}},
                 {&ft}, left_section, middle_section, right_section);

  Bar<decltype(left_section), decltype(middle_section), decltype(right_section)>
      right_bar({.x = 3840, .y = 0, .width = 1920, .height = 20},
                {.background{bgc}, .foreground{ft_fg}, .fg_accent{ft_acc}},
                {&ft}, left_section, middle_section, right_section);

  std::tuple tasks{
      ModuleTask(&workspaces, &left_bar, &middle_bar, &right_bar),
      ModuleTask(&windows, &left_bar, &middle_bar, &right_bar),
      ModuleTask(&clock, &left_bar, &middle_bar, &right_bar),
      Task(left_bar.get_event_handler()),
      Task(middle_bar.get_event_handler()),
      Task(right_bar.get_event_handler()),
  };
  while (true) {
    std::apply([](auto&... tasks) { ((tasks.work()), ...); }, tasks);

    // TODO: apply a better backoff heuristic. This is temporary.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}
