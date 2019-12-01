/** TODO
 * - Replace module calls to system where possible.
 * - Refactor Bars to work with multiple bars. Modules in a bar should not be
 *   required to be duplicated for each instantiation of a bar, they should be
 *   able to reference one common module to avoid duplication.
 * - Each module should have its own areas
 * - Handle bar events in addition to module events.
 * - Isolate X11 code into a WM module that can be swapped with something else
 *   like Wayland down the road.
 * - (1) Use _middle_pixmap in window instead of populating external pixmap.
 * - The pixmap patch seems to have added some bugs. Will segfault and print
 *   garbage seemingly at random.
 */

#include "bars.h"
#include "modules/workspaces.h"
#include "modules/windows.h"
#include "modules/clock.h"
#include "modules/fill.h"

int
main () {
  if (!XInitThreads()) {
    fprintf(stderr, "Failed to initialize threading for Xlib\n");
    exit(EXIT_FAILURE);
  }

  // rendering bug when using:
  /* Bars< */
  /*   bar_t< */
  /*     1920, 0, 1919, 20, */
  /*     section_t<mod_workspaces, */
  /*               mod_windows>, */
  /*     section_t<mod_clock, */
  /*               mod_clock, */
  /*               mod_workspaces, */
  /*               mod_windows, */
  /*               mod_clock, */
  /*               mod_windows>, */
  /*     section_t<> */
  /*   > */
  /* > bars; */

  Bars<
    bar_t<
      1920, 0, 1919, 20,         // config
      section_t<mod_fill<' '>,   // left
                mod_workspaces,
                mod_fill<'|', ' '>,
                mod_windows>,
      section_t<mod_clock>,      // center
      section_t<>                // right
    >
  > bars;
}
