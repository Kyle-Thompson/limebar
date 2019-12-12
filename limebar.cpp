/** TODO
 * - Replace module calls to system where possible.
 * - Each module should have its own areas
 * - Handle bar events in addition to module events.
 * - Isolate X11 code into a WM module that can be swapped with something else
 *   like Wayland down the road.
 * - Should each section have it's own pixmap to reduce the number of modules
 *   that need to be queried when another module changes?
 * - Introduce concept of fixed size modules where an update to the module
 *   doesn't require redrawing any of the other modules at all.
 */

#include "bars.h"
#include "modules/workspaces.h"
#include "modules/windows.h"
#include "modules/clock.h"
#include "modules/fill.h"

#include <tuple>
#include <X11/Xlib.h>

// TODO: Specialize on StaticModule to not spawn a thread
template <typename Mod>
class ModuleContainer {
 public:
  template <typename ...Args>
  ModuleContainer(Args ...args)
    : _module(std::forward<Args>(args)...)
    , _thread(std::ref(_module))
  {}

  ~ModuleContainer() {
    _thread.join();
  }

  Mod& operator*() { return _module; }

  Mod _module;
  std::thread _thread;
};

template <typename ...Mods>
std::tuple<Mods& ...> make_section(ModuleContainer<Mods>& ...mods) {
  return std::tuple<Mods& ...>{*mods...};
}

int
main () {
  if (!XInitThreads()) {
    fprintf(stderr, "Failed to initialize threading for Xlib\n");
    exit(EXIT_FAILURE);
  }

  ModuleContainer<mod_fill>       space(" ");
  ModuleContainer<mod_workspaces> workspaces;
  ModuleContainer<mod_fill>       sep("| ");
  ModuleContainer<mod_windows>    windows;
  ModuleContainer<mod_clock>      clock;

  auto left_section   { make_section(space, workspaces, sep, windows) };
  auto middle_section { make_section(clock) };
  auto right_section  { make_section() };

  bar_t left_bar(
    { .origin_x = 0, .origin_y = 0, .width = 1920, .height = 20 },
    left_section, middle_section, right_section
  );

  bar_t middle_bar(
    { .origin_x = 1920, .origin_y = 0, .width = 1920, .height = 20 },
    left_section, middle_section, right_section
  );

  bar_t right_bar(
    { .origin_x = 3840, .origin_y = 0, .width = 1920, .height = 20 },
    left_section, middle_section, right_section
  );

  Bars bars(left_bar, middle_bar, right_bar);
}
