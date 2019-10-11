#include "monitors.h"

#include "config.h"
#include "color.h"
#include "enums.h"
#include "x.h"

#include <optional>

Monitors* Monitors::instance = nullptr;

Monitors *
Monitors::Instance() {
  if (!instance) {
    instance = new Monitors();
  }
  return instance;
}

// TODO: give this a home
static uint32_t attrs = 0;

void
set_attribute (const char modifier, const char attribute)
{
  // indexof
  int pos = strchr("ou", attribute) - "ou";

  if (pos < 0) {
    fprintf(stderr, "Invalid attribute \"%c\" found\n", attribute);
    return;
  }

  switch (modifier) {
    case '+':
      attrs |= (1u<<pos);
      break;
    case '-':
      attrs &=~(1u<<pos);
      break;
    case '!':
      attrs ^= (1u<<pos);
      break;
  }
}

monitor_t::monitor_t(int x, int y, int width, int height,
    xcb_visualid_t visual, rgba_t bgc, xcb_colormap_t colormap)
  : _x(x)
  , _y((TOPBAR ? BAR_Y_OFFSET : height - BAR_HEIGHT - BAR_Y_OFFSET) + y)
  , _width(width)
  , _window(X::Instance()->generate_id())
  , _pixmap(X::Instance()->generate_id())
{
  const uint32_t mask[] { *bgc.val(), *bgc.val(), FORCE_DOCK,
    XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE, colormap };

  X::Instance()->create_window(_window, _x, _y, _width,
      XCB_WINDOW_CLASS_INPUT_OUTPUT, visual,
      XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT |
          XCB_CW_EVENT_MASK | XCB_CW_COLORMAP, mask);

  X::Instance()->create_pixmap(_pixmap, _window, _width);
}

int
monitor_t::shift(int x, int align, int ch_width) {
  switch (align) {
    case ALIGN_C:
      X::Instance()->copy_area(_pixmap, _pixmap, _width / 2 - x / 2,
                               _width / 2 - (x + ch_width) / 2, x);
      x = _width / 2 - (x + ch_width) / 2 + x;
      break;
    case ALIGN_R:
      X::Instance()->copy_area(_pixmap, _pixmap, _width - x,
                               _width - x - ch_width, x);
      x = _width - ch_width;
      break;
  }

  /* Draw the background first */
  X::Instance()->fill_rect(_pixmap, GC_CLEAR, x, 0, ch_width, BAR_HEIGHT);
  return x;
}

void
monitor_t::draw_lines(int x, int w) {
  /* We can render both at the same time */
  if (attrs & ATTR_OVERL)
    X::Instance().fill_rect(_pixmap, GC_ATTR, x, 0, w, UNDERLINE_HEIGHT);
  if (attrs & ATTR_UNDERL)
    X::Instance().fill_rect(_pixmap, GC_ATTR, x, BAR_HEIGHT - UNDERLINE_HEIGHT,
                             w, UNDERLINE_HEIGHT);
}

void
monitor_t::draw_shift(int x, int align, int w) {
  x = shift(x, align, w);
  draw_lines(x, w);
}

int
monitor_t::draw_char(font_t *cur_font, int x, int align, FcChar16 ch) {
  const int ch_width = X::Instance().xft_char_width(ch, cur_font->xft_ft);
  x = shift(x, align, ch_width);

  const int y = BAR_HEIGHT / 2 + cur_font->height / 2
                - cur_font->descent + cur_font->offset;
  X::Instance().xft_draw_string_16(cur_font->xft_ft, x, y, &ch, 1);
  draw_lines(x, ch_width);

  return ch_width;
}

Monitors::~Monitors() {
  // TODO: destruct individual monitors in monitor_t destructor
  for (const auto& mon : _monitors) {
    X::Instance().destroy_window(mon._window);
    X::Instance().free_pixmap(mon._pixmap);
  }
}

void
Monitors::init(std::vector<xcb_rectangle_t>& rects,
    xcb_visualid_t visual, rgba_t bgc, xcb_colormap_t colormap)
{
  int width = 0, height = 0;
  int left = BAR_X_OFFSET;

  // Sort before use
  std::sort(rects.begin(), rects.end(), [](const auto r1, const auto r2) {
    if (r1.x < r2.x || r1.y + r1.height <= r2.y) return -1;
    if (r1.x > r2.x || r1.y + r1.height > r2.y) return 1;
    return 0;
  });

  for (const auto& rect : rects) {
    int h = rect.y + rect.height;
    // Accumulated width of all monitors
    width += rect.width;
    // Get height of screen from y_offset + height of lowest monitor
    if (h >= height)
      height = h;
  }

  // Check the geometry
  if (BAR_X_OFFSET + BAR_WIDTH > width || BAR_Y_OFFSET + BAR_HEIGHT > height) {
    fprintf(stderr, "The geometry specified doesn't fit the screen!\n");
    exit(EXIT_FAILURE);
  }

  // Left is a positive number or zero therefore monitors with zero width are excluded
  width = BAR_WIDTH;
  for (auto& rect : rects) {
    if (rect.y + rect.height < BAR_Y_OFFSET)
      continue;
    if (rect.width > left) {
      _monitors.emplace_back(
          rect.x + left,
          rect.y,
          std::min(width, rect.width - left),
          rect.height,
          visual,
          bgc,
          colormap
          );

      width -= rect.width - left;
      // No need to check for other monitors
      if (width <= 0)
        break;
    }
    fflush(stdout);

    left -= std::min(static_cast<int>(rect.width), left);
  }
}

std::optional<area_t>
Monitors::area_get(xcb_window_t win, const int btn, const int x) {
  auto pred = [&](area_t a) {
    return (a.window == win && a.button == btn && x >= a.begin && x < a.end);
  };
  // TODO: binary search?
  // TODO: why do we need reverse iterators?
  auto itr = std::find_if(_areas.rbegin(), _areas.rend(), pred);
  return (itr != _areas.rend()) ? std::optional<area_t>{*itr} : std::nullopt;
}

void
Monitors::area_shift (xcb_window_t win, const int align, int delta) {
  if (align == ALIGN_L)
    return;
  if (align == ALIGN_C)
    delta /= 2;

  for (auto itr = _areas.begin(); itr != _areas.end(); ++itr) {
    if (itr->window == win && itr->align == align && !itr->active) {
      itr->begin -= delta;
      itr->end -= delta;
    }
  }
}

bool
Monitors::area_add(char *str, const char *optend, char **end, monitor_t *mon,
    const uint16_t x, const int8_t align, const uint8_t button)
{
  // A wild close area tag appeared!
  if (*str != ':') {
    *end = str;

    // Find most recent unclosed area.
    auto ritr = _areas.rbegin();
    while (ritr != _areas.rend() && !ritr->active)
      ++ritr;

    // Basic safety checks
    if (!ritr->cmd || ritr->align != align || ritr->window != mon->_window) {
      fprintf(stderr, "Invalid geometry for the clickable area\n");
      return false;
    }

    const int size = x - ritr->begin;
    switch (align) {
      case ALIGN_L:
        ritr->end = x;
        break;
      case ALIGN_C:
        ritr->begin = mon->_width / 2 - size / 2 + ritr->begin / 2;
        ritr->end = ritr->begin + size;
        break;
      case ALIGN_R:
        // The newest is the rightmost one
        ritr->begin = mon->_width - size;
        ritr->end = mon->_width;
        break;
    }

    ritr->active = false;
    return true;
  }

  // Found the closing : and check if it's just an escaped one
  char *trail;
  for (trail = strchr(++str, ':'); trail && trail[-1] == '\\';
      trail = strchr(trail + 1, ':'))
    ;

  // Find the trailing : and make sure it's within the formatting block, also
  // reject empty commands
  if (!trail || str == trail || trail > optend) {
    *end = str;
    return false;
  }

  *trail = '\0';

  // Sanitize the user command by unescaping all the :
  for (char *needle = str; *needle; needle++) {
    int delta = trail - &needle[1];
    if (needle[0] == '\\' && needle[1] == ':') {
      memmove(&needle[0], &needle[1], delta);
      needle[delta] = 0;
    }
  }

  _areas.push_back({
    .begin = x,
    .active = true,
    .align = align,
    .button = button,
    .window = mon->_window,
    .cmd = str,
  });

  *end = trail + 1;

  return true;
}
