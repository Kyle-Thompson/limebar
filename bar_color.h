#pragma once

#include "color.h"
#include "config.h"

/** BarColors
 * Collection of all colors relevant to the bar.
 */
struct BarColors {
  rgba_t background;
  typename DS::font_color foreground;
  typename DS::font_color fg_accent;
};
