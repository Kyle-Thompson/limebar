/** TODO
 * - Replace module calls to system where possible.
 * - Create a pixmap and window class with constructors and destructors rather
 *   than having the bar class deal with it.
 * - Refactor Bars to work with multiple bars. Modules in a bar should not be
 *   required to be duplicated for each instantiation of a bar, they should be
 *   able to reference one common module to avoid duplication.
 * - Each module should have its own areas
 * - Now that strings returned from modules are only textually parsed there is
 *   no way to do accent highlighting. Find a way to do accents in this model or
 *   find a new model.
 */

#include "bars.h"
#include "modules/workspaces.h"
#include "modules/windows.h"
#include "modules/clock.h"

int
main () {
  if (!XInitThreads()) {
    fprintf(stderr, "Failed to initialize threading for Xlib\n");
    exit(EXIT_FAILURE);
  }

  Bars<
    bar_t<
      1920, 0, 1920, 20,         // config
      section_t<mod_workspaces,  // left
                mod_windows>,
      section_t<mod_clock>,      // center
      section_t<mod_clock>       // right
    >
  > bars;
}
