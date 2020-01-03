/** TODO
 * - Replace module calls to system where possible.
 * - Each module should have its own areas
 * - Handle bar events in addition to module events.
 * - Isolate X11 code into a DS module that can be swapped with something else
 *   like Wayland down the road.
 * - Should each section have it's own pixmap to reduce the number of modules
 *   that need to be queried when another module changes?
 * - Introduce concept of fixed size modules where an update to the module
 *   doesn't require redrawing any of the other modules at all.
 * - Fix startup race condition where some bars might not have an initial value.
 * - Add option for padding around text in the bar.
 * - Replace current use of module notifications via condvars with work queues.
 */

#include "bars.h"
#include "color.h"
#include "config.h"
#include "font.h"
#include "modules/module.h"
#include "modules/workspaces.h"
#include "modules/windows.h"
#include "modules/clock.h"
#include "modules/fill.h"

#include <iostream>
#include <tuple>
#include <X11/Xlib.h>

int
main () {
  DS::init();

  ModuleContainer<mod_fill>       space(" ");
  ModuleContainer<mod_workspaces> workspaces;
  ModuleContainer<mod_fill>       sep("| ");
  ModuleContainer<mod_windows>    windows;
  ModuleContainer<mod_clock>      clock;

  auto left_section   { make_section(space, workspaces, sep, windows) };
  auto middle_section { make_section(clock) };
  auto right_section  { make_section() };

  X& x11 = X::Instance();

  rgba_t bgc = rgba_t::parse(x11.get_string_resource("background").c_str());
  rgba_t fgc = rgba_t::parse(x11.get_string_resource("foreground").c_str());
  rgba_t acc = rgba_t::parse(x11.get_string_resource("color4").c_str());

  DS::font_t ft("GohuFont:pixelsize=11");

  Bar left_bar(
    { .origin_x = 0, .origin_y = 0, .width = 1920, .height = 20 },
    { .background = bgc, .foreground = fgc, .fg_accent = acc },
    { &ft }, left_section, middle_section, right_section);

  Bar middle_bar(
    { .origin_x = 1920, .origin_y = 0, .width = 1920, .height = 20 },
    { .background = bgc, .foreground = fgc, .fg_accent = acc },
    { &ft }, left_section, middle_section, right_section);

  Bar right_bar(
    { .origin_x = 3840, .origin_y = 0, .width = 1920, .height = 20 },
    { .background = bgc, .foreground = fgc, .fg_accent = acc },
    { &ft }, left_section, middle_section, right_section);

  BarContainer bars(left_bar, middle_bar, right_bar);
}
