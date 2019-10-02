#include "monitors.h"

#include "config.h"
#include "color.h"
#include "x.h"

Monitors::~Monitors() {
  // TODO: destruct individual monitors in monitor_t destructor
  for (const auto& mon : _monitors) {
    xcb_destroy_window(X::Instance()->get_connection(), mon._window);
    xcb_free_pixmap(X::Instance()->get_connection(), mon._pixmap);
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
  auto *scr = X::Instance()->get_screen();
  int depth = (visual == scr->root_visual) ? XCB_COPY_FROM_PARENT : 32;
  const uint32_t mask[] { *bgc.val(), *bgc.val(), FORCE_DOCK,
    XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE, colormap };
  xcb_create_window(X::Instance()->get_connection(), depth, _window, scr->root,
      _x, _y, _width, BAR_HEIGHT, 0,
      XCB_WINDOW_CLASS_INPUT_OUTPUT, visual,
      XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP,
      mask);

  xcb_create_pixmap(X::Instance()->get_connection(), depth, _pixmap, _window, _width, BAR_HEIGHT);
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

    left -= std::min(static_cast<int>(rect.width), left);
  }
}
