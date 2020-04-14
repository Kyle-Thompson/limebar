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
  static mod_fill space(" ");
  static mod_workspaces workspaces;
  static mod_fill sep("| ");
  static mod_windows windows;
  static mod_clock clock;

  constexpr auto builder =
      BarBuilderHelper()
          .bg_bar_color_from_rdb("background")
          .fg_font_color_from_rdb("foreground")
          .acc_font_color_from_rdb("color4")
          .font_from_rdb("font")
          .left(section_wrapper(space, workspaces, sep, windows))
          .middle(section_wrapper(clock))
          .right(section_wrapper());

  constexpr size_t W = 1920;
  constexpr size_t H = 20;
  Bar l = build(builder.area({.x = 0,     .y = 0, .width = W, .height = H}));
  Bar m = build(builder.area({.x = W,     .y = 0, .width = W, .height = H}));
  Bar r = build(builder.area({.x = W * 2, .y = 0, .width = W, .height = H}));

  std::tuple tasks{
      ModuleTask(&workspaces, &l, &m, &r), ModuleTask(&windows, &l, &m, &r),
      ModuleTask(&clock, &l, &m, &r),      Task(l.get_event_handler()),
      Task(m.get_event_handler()),         Task(r.get_event_handler())};

  while (true) {
    std::apply([](auto&... task) { ((task.work()), ...); }, tasks);

    // TODO: apply a better backoff heuristic. This is temporary.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}
