// vim:sw=2:ts=2:et:

/** TODO
 * - Convert global variables into Singletons.
 * - Modules should return pixmaps or a format that does not need to be parsed
 *   but is instead sent directly to the bar.
 * - Find more ergonomic way to reference singletons.
 * - Add more functions into DisplayManager singleton. Too many raw calls
 *   happening here that should be members.
 * - Use static polymorphism with modules.
 */

#include "config.h"
#include "DisplayManager.h"
#include "fonts.h"
#include "x.h"
#include "modules/module.h"
#include "modules/windows.h"
#include "modules/workspaces.h"
#include "modules/clock.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <condition_variable>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>
#include <xcb/randr.h>
#include <xcb/xcb_xrm.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xatom.h>


struct area_t {
  uint16_t begin;
  uint16_t end;
  bool active:1;
  int8_t align:3;
  uint8_t button:3;
  xcb_window_t window;
  char *cmd;
};

struct rgba_t {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;

  rgba_t() = default;
  rgba_t(uint32_t v) { set(v); }
  rgba_t(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : r(r), g(g), b(b), a(a) {}
  void set(uint32_t v) { memcpy(this, &v, sizeof(v)); }
  uint32_t* val() { return reinterpret_cast<uint32_t*>(this); }
};

struct monitor_t {
  // TODO: simplify constructor with internal references to singletons
  monitor_t(int x, int y, int width, int height, xcb_screen_t *scr, xcb_visualid_t visual, rgba_t bgc, xcb_colormap_t colormap)
    : _x(x)
    , _y((TOPBAR ? BAR_Y_OFFSET : height - BAR_HEIGHT - BAR_Y_OFFSET) + y)
    , _width(width)
    , _window(X::Instance()->generate_id())
    , _pixmap(X::Instance()->generate_id())
  {
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

  ~monitor_t() {
    // DisplayManager::Instance()->xcb_destroy_window(_window);
    // DisplayManager::Instance()->xcb_destroy_window(_pixmap);
  }

  int _x, _y, _width;
  xcb_window_t _window;
  xcb_pixmap_t _pixmap;
};

enum {
  ATTR_OVERL = (1<<0),
  ATTR_UNDERL = (1<<1),
};

enum {
  ALIGN_L = 0,
  ALIGN_C,
  ALIGN_R
};

enum {
  GC_DRAW = 0,
  GC_CLEAR,
  GC_ATTR,
  GC_MAX
};


static xcb_screen_t *scr;
static int scr_nbr = 0;

static xcb_gcontext_t gc[GC_MAX];
static xcb_visualid_t visual;
static Visual *visual_ptr;
static xcb_colormap_t colormap;

static uint32_t attrs = 0;
static rgba_t fgc, bgc, ugc;

static std::vector<area_t> areas;

static XftColor sel_fg;
static XftDraw *xft_draw;

//char width lookuptable
constexpr size_t MAX_WIDTHS {1 << 16};
static wchar_t xft_char[MAX_WIDTHS];
static char    xft_width[MAX_WIDTHS];

static Fonts fonts;

struct Monitors {
  void init(std::vector<xcb_rectangle_t>& rects);

  std::vector<monitor_t>::iterator begin() { return _monitors.begin(); }
  std::vector<monitor_t>::iterator end()   { return _monitors.end(); }
  std::vector<monitor_t>::const_iterator cbegin() { return _monitors.cbegin(); }
  std::vector<monitor_t>::const_iterator cend()   { return _monitors.cend(); }

  std::vector<monitor_t> _monitors;
};

void Monitors::init(std::vector<xcb_rectangle_t>& rects)
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
          scr,
          visual,
          bgc,
          colormap
          );

      width -= rect.width - left;
      // No need to check for other monitors
      if (width <= 0)
        break;
    }

    left -= rect.width;

    if (left < 0)
      left = 0;
  }
}

Monitors monitors;


static const auto modules = [] {
  std::unordered_map<const char*, std::unique_ptr<module>> modules;
  modules.emplace("workspaces", std::make_unique<mod_workspaces>());
  modules.emplace("windows", std::make_unique<mod_windows>());
  modules.emplace("clock", std::make_unique<mod_clock>());
  return modules;
}();


void
update_gc ()
{
  xcb_change_gc(X::Instance()->get_connection(), gc[GC_DRAW], XCB_GC_FOREGROUND, fgc.val());
  xcb_change_gc(X::Instance()->get_connection(), gc[GC_CLEAR], XCB_GC_FOREGROUND, bgc.val());
  xcb_change_gc(X::Instance()->get_connection(), gc[GC_ATTR], XCB_GC_FOREGROUND, ugc.val());
  DisplayManager::Instance()->xft_color_free(visual_ptr, colormap, &sel_fg);
  char color[] = "#ffffff";
  uint32_t nfgc = *fgc.val() & 0x00ffffff;
  snprintf(color, sizeof(color), "#%06X", nfgc);
  if (!DisplayManager::Instance()->xft_color_alloc_name(visual_ptr, colormap, color, &sel_fg)) {
    fprintf(stderr, "Couldn't allocate xft font color '%s'\n", color);
  }
}

void
fill_rect (xcb_drawable_t d, xcb_gcontext_t _gc, int16_t x, int16_t y, uint16_t width, uint16_t height)
{
  xcb_rectangle_t rect = { x, y, width, height };
  xcb_poly_fill_rectangle(X::Instance()->get_connection(), d, _gc, 1, &rect);
}

// Apparently xcb cannot seem to compose the right request for this call, hence we have to do it by
// ourselves.
// The funcion is taken from 'wmdia' (http://wmdia.sourceforge.net/)
xcb_void_cookie_t
xcb_poly_text_16_simple(xcb_connection_t * c, xcb_drawable_t drawable,
    xcb_gcontext_t gc, int16_t x, int16_t y, uint32_t len, const uint16_t *str)
{
  static const xcb_protocol_request_t xcb_req = {
    5,                // count
    nullptr,          // ext
    XCB_POLY_TEXT_16, // opcode
    1                 // isvoid
  };
  struct iovec xcb_parts[7];
  uint8_t xcb_lendelta[2];
  xcb_void_cookie_t xcb_ret;
  xcb_poly_text_8_request_t xcb_out;

  xcb_out.pad0 = 0;
  xcb_out.drawable = drawable;
  xcb_out.gc = gc;
  xcb_out.x = x;
  xcb_out.y = y;

  xcb_lendelta[0] = len;
  xcb_lendelta[1] = 0;

  xcb_parts[2].iov_base = (char *)&xcb_out;
  xcb_parts[2].iov_len = sizeof(xcb_out);
  xcb_parts[3].iov_base = nullptr;
  xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

  xcb_parts[4].iov_base = xcb_lendelta;
  xcb_parts[4].iov_len = sizeof(xcb_lendelta);
  xcb_parts[5].iov_base = (char *)str;
  xcb_parts[5].iov_len = len * sizeof(int16_t);

  xcb_parts[6].iov_base = nullptr;
  xcb_parts[6].iov_len = -(xcb_parts[4].iov_len + xcb_parts[5].iov_len) & 3;

  xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);

  return xcb_ret;
}

int
xft_char_width (uint16_t ch, font_t *cur_font)
{
  const int slot = [](uint16_t ch) {
    int slot = ch % MAX_WIDTHS;
    while (xft_char[slot] != 0 && xft_char[slot] != ch)
    {
      slot = (slot + 1) % MAX_WIDTHS;
    }
    return slot;
  }(ch);

  if (!xft_char[slot]) {
    XGlyphInfo gi;
    FT_UInt glyph = XftCharIndex (DisplayManager::Instance()->get_display(), cur_font->xft_ft, (FcChar32) ch);
    XftFontLoadGlyphs (DisplayManager::Instance()->get_display(), cur_font->xft_ft, FcFalse, &glyph, 1);
    XftGlyphExtents (DisplayManager::Instance()->get_display(), cur_font->xft_ft, &glyph, 1, &gi);
    XftFontUnloadGlyphs (DisplayManager::Instance()->get_display(), cur_font->xft_ft, &glyph, 1);
    xft_char[slot] = ch;
    xft_width[slot] = gi.xOff;
    return gi.xOff;
  } else if (xft_char[slot] == ch)
    return xft_width[slot];
  else
    return 0;
}

int
shift (monitor_t *mon, int x, int align, int ch_width)
{
  switch (align) {
    case ALIGN_C:
      xcb_copy_area(X::Instance()->get_connection(), mon->_pixmap, mon->_pixmap, gc[GC_DRAW],
          mon->_width / 2 - x / 2, 0,
          mon->_width / 2 - (x + ch_width) / 2, 0,
          x, BAR_HEIGHT);
      x = mon->_width / 2 - (x + ch_width) / 2 + x;
      break;
    case ALIGN_R:
      xcb_copy_area(X::Instance()->get_connection(), mon->_pixmap, mon->_pixmap, gc[GC_DRAW],
          mon->_width - x, 0,
          mon->_width - x - ch_width, 0,
          x, BAR_HEIGHT);
      x = mon->_width - ch_width;
      break;
  }

  /* Draw the background first */
  fill_rect(mon->_pixmap, gc[GC_CLEAR], x, 0, ch_width, BAR_HEIGHT);
  return x;
}

void
draw_lines (monitor_t *mon, int x, int w)
{
  /* We can render both at the same time */
  if (attrs & ATTR_OVERL)
    fill_rect(mon->_pixmap, gc[GC_ATTR], x, 0, w, UNDERLINE_HEIGHT);
  if (attrs & ATTR_UNDERL)
    fill_rect(mon->_pixmap, gc[GC_ATTR], x, BAR_HEIGHT - UNDERLINE_HEIGHT, w, UNDERLINE_HEIGHT);
}

void
draw_shift (monitor_t *mon, int x, int align, int w)
{
  x = shift(mon, x, align, w);
  draw_lines(mon, x, w);
}

int
draw_char (monitor_t *mon, font_t *cur_font, int x, int align, uint16_t ch)
{
  int ch_width;

  if (cur_font->xft_ft) {
    ch_width = xft_char_width(ch, cur_font);
  } else {
    ch_width = (cur_font->width_lut) ?
      cur_font->width_lut[ch - cur_font->char_min].character_width:
      cur_font->width;
  }

  x = shift(mon, x, align, ch_width);

  int y = BAR_HEIGHT / 2 + cur_font->height / 2 - cur_font->descent + fonts.current()->offset;
  if (cur_font->xft_ft) {
    XftDrawString16 (xft_draw, &sel_fg, cur_font->xft_ft, x,y, &ch, 1);
  } else {
    /* xcb accepts string in UCS-2 BE, so swap */
    ch = (ch >> 8) | (ch << 8);

    // The coordinates here are those of the baseline
    xcb_poly_text_16_simple(X::Instance()->get_connection(), mon->_pixmap, gc[GC_DRAW],
        x, y,
        1, &ch);
  }

  draw_lines(mon, x, ch_width);

  return ch_width;
}

rgba_t
parse_color (const char *str, char **end)
{
  static const rgba_t ERR_COLOR { 0xffffffffU };

  int string_len;
  char *ep;

  if (!str)
    return ERR_COLOR;

  // Reset
  if (str[0] == '-') {
    if (end)
      *end = (char *)str + 1;

    return ERR_COLOR;
  }

  // Hex representation
  if (str[0] != '#') {
    if (end)
      *end = (char *)str;

    fprintf(stderr, "Invalid color specified\n");
    return ERR_COLOR;
  }

  errno = 0;
  rgba_t tmp { (uint32_t)strtoul(str + 1, &ep, 16) };

  if (end)
    *end = ep;

  // Some error checking is definitely good
  if (errno) {
    fprintf(stderr, "Invalid color specified\n");
    return ERR_COLOR;
  }

  string_len = ep - (str + 1);

  switch (string_len) {
    case 3:
      // Expand the #rgb format into #rrggbb (aa is set to 0xff)
      tmp.set((*tmp.val() & 0xf00) * 0x1100
            | (*tmp.val() & 0x0f0) * 0x0110
            | (*tmp.val() & 0x00f) * 0x0011);
      [[fallthrough]];
    case 6:
      // If the code is in #rrggbb form then assume it's opaque
      tmp.a = 255;
      break;
    case 7:
    case 8:
      // Colors in #aarrggbb format, those need no adjustments
      break;
    default:
      fprintf(stderr, "Invalid color specified\n");
      return ERR_COLOR;
  }

  // Premultiply the alpha in
  if (tmp.a) {
    // The components are clamped automagically as the rgba_t is made of uint8_t
    return {
      static_cast<uint8_t>((tmp.r * tmp.a) / 255),
      static_cast<uint8_t>((tmp.g * tmp.a) / 255),
      static_cast<uint8_t>((tmp.b * tmp.a) / 255),
      tmp.a,
    };
  }

  return { 0U };
}

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


std::optional<area_t>
area_get (xcb_window_t win, const int btn, const int x)
{
  auto pred = [&](area_t a) {
    return (a.window == win && a.button == btn && x >= a.begin && x < a.end);
  };
  // TODO: binary search?
  auto itr = std::find_if(areas.rbegin(), areas.rend(), pred);
  return (itr != areas.rend()) ? std::optional<area_t>{*itr} : std::nullopt;
}

void
area_shift (xcb_window_t win, const int align, int delta)
{
  if (align == ALIGN_L)
    return;
  if (align == ALIGN_C)
    delta /= 2;

  for (auto itr = areas.begin(); itr != areas.end(); ++itr) {
    if (itr->window == win && itr->align == align && !itr->active) {
      itr->begin -= delta;
      itr->end -= delta;
    }
  }
}

bool
area_add (char *str, const char *optend, char **end, monitor_t *mon,
    const uint16_t x, const int8_t align, const uint8_t button)
{
  // A wild close area tag appeared!
  if (*str != ':') {
    *end = str;

    // Find most recent unclosed area.
    auto ritr = areas.rbegin();
    while (ritr != areas.rend() && !ritr->active)
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

  areas.push_back({
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

void
parse (char *text)
{
  font_t *cur_font;
  int pos_x, align, button;
  char *p = text, *block_end, *ep;
  rgba_t tmp;

  pos_x = 0;
  align = ALIGN_L;
  auto mon_itr = monitors.begin();

  areas.clear();

  for (const monitor_t& m : monitors)
    fill_rect(m._pixmap, gc[GC_CLEAR], 0, 0, m._width, BAR_HEIGHT);

  /* Create xft drawable */
  if (!(xft_draw = (DisplayManager::Instance()->xft_draw_create(mon_itr->_pixmap, visual_ptr, colormap)))) {
    fprintf(stderr, "Couldn't create xft drawable\n");
  }

  for (;;) {
    if (*p == '\0' || *p == '\n')
      break;

    if (p[0] == '%' && p[1] == '{' && (block_end = strchr(p++, '}'))) {
      p++;
      while (p < block_end) {
        int w;
        while (isspace(*p))
          p++;

        switch (*p++) {
          case '+': set_attribute('+', *p++); break;
          case '-': set_attribute('-', *p++); break;
          case '!': set_attribute('!', *p++); break;

          case 'R':
            tmp = fgc;
            fgc = bgc;
            bgc = tmp;
            update_gc();
            break;

          case 'l': pos_x = 0; align = ALIGN_L; break;
          case 'c': pos_x = 0; align = ALIGN_C; break;
          case 'r': pos_x = 0; align = ALIGN_R; break;

          case 'A':
            button = XCB_BUTTON_INDEX_1;
            // The range is 1-5
            if (isdigit(*p) && (*p > '0' && *p < '6'))
              button = *p++ - '0';
            if (!area_add(p, block_end, &p, &*mon_itr, pos_x, align, button))
              return;
            break;

          case 'B': bgc = parse_color(p, &p); update_gc(); break;
          case 'F': fgc = parse_color(p, &p); update_gc(); break;
          case 'U': ugc = parse_color(p, &p); update_gc(); break;

          case 'S':
            if (*p == '+' && (mon_itr + 1) != monitors.end()) {
              ++mon_itr;
            } else if (*p == '-' && mon_itr != monitors.begin()) {
              --mon_itr;
            } else if (*p == 'f') {
              mon_itr = monitors.begin();
            } else if (*p == 'l') {
              mon_itr = --monitors.end();
            } else if (isdigit(*p)) {
              mon_itr = monitors.begin() + *p-'0';
            } else {
              p++;
              continue;
            }

            XftDrawDestroy (xft_draw);
            if (!(xft_draw = DisplayManager::Instance()->xft_draw_create(mon_itr->_pixmap, visual_ptr , colormap ))) {
              fprintf(stderr, "Couldn't create xft drawable\n");
            }

            p++;
            pos_x = 0;
            break;
          case 'O':
            errno = 0;
            w = (int) strtoul(p, &p, 10);
            if (errno)
              continue;

            draw_shift(&*mon_itr, pos_x, align, w);

            pos_x += w;
            area_shift(mon_itr->_window, align, w);
            break;

          case 'T':
            if (*p == '-') { //Reset to automatic font selection
              fonts._index = -1;
              p++;
              break;
            } else if (isdigit(*p)) {
              fonts._index = (int)strtoul(p, &ep, 10);
              // User-specified 'font_index' ∊ (0,font_count]
              // Otherwise just fallback to the automatic font selection
              if (fonts._index < 0 || fonts._index > FONTS.size())
                fonts._index = -1;
              p = ep;
              break;
            } else {
              fprintf(stderr, "Invalid font slot \"%c\"\n", *p++); //Swallow the token
              break;
            }

            // In case of error keep parsing after the closing }
          default:
            p = block_end;
        }
      }
      // Eat the trailing }
      p++;
    } else { // utf-8 -> ucs-2
      uint8_t *utf = (uint8_t *)p;
      uint16_t ucs;

      // ASCII
      if (utf[0] < 0x80) {
        ucs = utf[0];
        p  += 1;
      }
      // Two byte utf8 sequence
      else if ((utf[0] & 0xe0) == 0xc0) {
        ucs = (utf[0] & 0x1f) << 6 | (utf[1] & 0x3f);
        p += 2;
      }
      // Three byte utf8 sequence
      else if ((utf[0] & 0xf0) == 0xe0) {
        ucs = (utf[0] & 0xf) << 12 | (utf[1] & 0x3f) << 6 | (utf[2] & 0x3f);
        p += 3;
      }
      // Four byte utf8 sequence
      else if ((utf[0] & 0xf8) == 0xf0) {
        ucs = 0xfffd;
        p += 4;
      }
      // Five byte utf8 sequence
      else if ((utf[0] & 0xfc) == 0xf8) {
        ucs = 0xfffd;
        p += 5;
      }
      // Six byte utf8 sequence
      else if ((utf[0] & 0xfe) == 0xfc) {
        ucs = 0xfffd;
        p += 6;
      }
      // Not a valid utf-8 sequence
      else {
        ucs = utf[0];
        p += 1;
      }

      cur_font = fonts.select_drawable_font(ucs);
      if (!cur_font)
        continue;

      if(cur_font->ptr)
        xcb_change_gc(X::Instance()->get_connection(), gc[GC_DRAW] , XCB_GC_FONT, &cur_font->ptr);
      int w = draw_char(&*mon_itr, cur_font, pos_x, align, ucs);

      pos_x += w;
      area_shift(mon_itr->_window, align, w);
    }
  }
  XftDrawDestroy (xft_draw);
}

void
set_ewmh_atoms ()
{
  enum {
    NET_WM_WINDOW_TYPE,
    NET_WM_WINDOW_TYPE_DOCK,
    NET_WM_DESKTOP,
    NET_WM_STRUT_PARTIAL,
    NET_WM_STRUT,
    NET_WM_STATE,
    NET_WM_STATE_STICKY,
    NET_WM_STATE_ABOVE,
  };

  static constexpr size_t size = 8;
  static constexpr std::array<const char *, size> atom_names {
    "_NET_WM_WINDOW_TYPE",
    "_NET_WM_WINDOW_TYPE_DOCK",
    "_NET_WM_DESKTOP",
    "_NET_WM_STRUT_PARTIAL",
    "_NET_WM_STRUT",
    "_NET_WM_STATE",
    // Leave those at the end since are batch-set
    "_NET_WM_STATE_STICKY",
    "_NET_WM_STATE_ABOVE",
  };
  std::array<xcb_intern_atom_cookie_t, size> atom_cookies;
  std::array<xcb_atom_t, size> atom_list;
  xcb_intern_atom_reply_t *atom_reply;

  // As suggested fetch all the cookies first (yum!) and then retrieve the
  // atoms to exploit the async'ness
  std::transform(atom_names.begin(), atom_names.end(), atom_cookies.begin(), [](auto name){
    return xcb_intern_atom(X::Instance()->get_connection(), 0, strlen(name), name);
  });

  for (int i = 0; i < atom_names.size(); i++) {
    atom_reply = xcb_intern_atom_reply(X::Instance()->get_connection(), atom_cookies[i], nullptr);
    if (!atom_reply)
      return;
    atom_list[i] = atom_reply->atom;
    free(atom_reply);
  }

  // Prepare the strut array
  for (const auto& mon : monitors) {
    int strut[12] = {0};
    if (TOPBAR) {
      strut[2] = BAR_HEIGHT;
      strut[8] = mon._x;
      strut[9] = mon._x + mon._width;
    } else {
      strut[3]  = BAR_HEIGHT;
      strut[10] = mon._x;
      strut[11] = mon._x + mon._width;
    }

    X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window, atom_list[NET_WM_WINDOW_TYPE], XCB_ATOM_ATOM, 32, 1, &atom_list[NET_WM_WINDOW_TYPE_DOCK]);
    X::Instance()->change_property(XCB_PROP_MODE_APPEND,  mon._window, atom_list[NET_WM_STATE], XCB_ATOM_ATOM, 32, 2, &atom_list[NET_WM_STATE_STICKY]);
    X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window, atom_list[NET_WM_DESKTOP], XCB_ATOM_CARDINAL, 32, 1, (const uint32_t []) { 0u - 1u } );
    X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window, atom_list[NET_WM_STRUT_PARTIAL], XCB_ATOM_CARDINAL, 32, 12, strut);
    X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window, atom_list[NET_WM_STRUT], XCB_ATOM_CARDINAL, 32, 4, strut);
    X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 3, "bar");
    X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, 12, "lemonbar\0Bar");
  }
}

void
get_randr_monitors ()
{
  xcb_randr_get_screen_resources_current_reply_t *rres_reply;
  xcb_randr_output_t *outputs;
  int num;

  rres_reply = xcb_randr_get_screen_resources_current_reply(X::Instance()->get_connection(),
      xcb_randr_get_screen_resources_current(X::Instance()->get_connection(), scr->root), nullptr);

  if (!rres_reply) {
    fprintf(stderr, "Failed to get current randr screen resources\n");
    return;
  }

  num = xcb_randr_get_screen_resources_current_outputs_length(rres_reply);
  outputs = xcb_randr_get_screen_resources_current_outputs(rres_reply);


  // There should be at least one output
  if (num < 1) {
    free(rres_reply);
    return;
  }

  std::vector<xcb_rectangle_t> rects;

  // Get all outputs
  for (int i = 0; i < num; i++) {
    xcb_randr_get_output_info_reply_t *oi_reply;
    xcb_randr_get_crtc_info_reply_t *ci_reply;

    oi_reply = xcb_randr_get_output_info_reply(X::Instance()->get_connection(),
        xcb_randr_get_output_info(X::Instance()->get_connection(), outputs[i], XCB_CURRENT_TIME),
        nullptr);

    // don't attach outputs that are disconnected or not attached to any CTRC
    if (!oi_reply || oi_reply->crtc == XCB_NONE || oi_reply->connection != XCB_RANDR_CONNECTION_CONNECTED) {
      free(oi_reply);
      continue;
    }

    ci_reply = xcb_randr_get_crtc_info_reply(X::Instance()->get_connection(),
        xcb_randr_get_crtc_info(X::Instance()->get_connection(), oi_reply->crtc, XCB_CURRENT_TIME), nullptr);

    free(oi_reply);

    if (!ci_reply) {
      fprintf(stderr, "Failed to get RandR ctrc info\n");
      free(rres_reply);
      return;
    }

    // There's no need to handle rotated screens here (see #69)
    if (ci_reply->width > 0)
      rects.push_back({ ci_reply->x, ci_reply->y, ci_reply->width, ci_reply->height });

    free(ci_reply);
  }

  free(rres_reply);

  if (rects.empty()) {
    fprintf(stderr, "No usable RandR output found\n");
    return;
  }

  monitors.init(rects);
}

xcb_visualid_t
get_visual ()
{

  XVisualInfo xv; 
  xv.depth = 32;
  int result = 0;
  XVisualInfo* result_ptr = nullptr; 
  result_ptr = DisplayManager::Instance()->get_visual_info(VisualDepthMask, &xv, &result);

  if (result > 0) {
    visual_ptr = result_ptr->visual;
    return result_ptr->visualid;
  }

  //Fallback
  visual_ptr = DisplayManager::Instance()->xft_default_visual(scr_nbr);
  return scr->root_visual;
}

void
xconn ()
{
  /* Grab infos from the first screen */
  scr = xcb_setup_roots_iterator(xcb_get_setup(X::Instance()->get_connection())).data;

  /* Try to get a RGBA visual and build the colormap for that */
  visual = get_visual();
  colormap = xcb_generate_id(X::Instance()->get_connection());
  xcb_create_colormap(X::Instance()->get_connection(), XCB_COLORMAP_ALLOC_NONE, colormap, scr->root, visual);
}

void
init ()
{
  fonts.init(X::Instance()->get_connection(), scr_nbr);

  char *val;
  X::Instance()->get_string_resource("background", &val);
  bgc = parse_color(val, nullptr);
  X::Instance()->get_string_resource("foreground", &val);
  ugc = fgc = parse_color(val, nullptr);

  // Generate a list of screens
  const xcb_query_extension_reply_t *qe_reply;

  // Check if RandR is present
  qe_reply = xcb_get_extension_data(X::Instance()->get_connection(), &xcb_randr_id);

  if (qe_reply && qe_reply->present) {
    get_randr_monitors();
  }

  // For WM that support EWMH atoms
  set_ewmh_atoms();

  // Create the gc for drawing
  gc[GC_DRAW] = xcb_generate_id(X::Instance()->get_connection());
  xcb_create_gc(X::Instance()->get_connection(), gc[GC_DRAW], monitors.begin()->_pixmap, XCB_GC_FOREGROUND, fgc.val());

  gc[GC_CLEAR] = xcb_generate_id(X::Instance()->get_connection());
  xcb_create_gc(X::Instance()->get_connection(), gc[GC_CLEAR], monitors.begin()->_pixmap, XCB_GC_FOREGROUND, bgc.val());

  gc[GC_ATTR] = xcb_generate_id(X::Instance()->get_connection());
  xcb_create_gc(X::Instance()->get_connection(), gc[GC_ATTR], monitors.begin()->_pixmap, XCB_GC_FOREGROUND, ugc.val());

  // Make the bar visible and clear the pixmap
  for (const auto& mon : monitors) {
    fill_rect(mon._pixmap, gc[GC_CLEAR], 0, 0, mon._width, BAR_HEIGHT);
    xcb_map_window(X::Instance()->get_connection(), mon._window);

    // Make sure that the window really gets in the place it's supposed to be
    // Some WM such as Openbox need this
    const uint32_t xy[] { static_cast<uint32_t>(mon._x), static_cast<uint32_t>(mon._y) };
    xcb_configure_window(X::Instance()->get_connection(), mon._window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, xy);

    // Set the WM_NAME atom to the user specified value
    if constexpr (WM_NAME != nullptr)
      X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window,
          XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(WM_NAME), WM_NAME);

    // set the WM_CLASS atom instance to the executable name
    if (WM_CLASS.size()) {
      constexpr int size = WM_CLASS.size() + 6;
      char wm_class[size] = {0};

      // WM_CLASS is nullbyte seperated: WM_CLASS + "\0Bar\0"
      strncpy(wm_class, WM_CLASS.data(), WM_CLASS.size());
      strcpy(wm_class + WM_CLASS.size(), "\0Bar");

      X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window,
          XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, size, wm_class);
    }
  }

  char color[] = "#ffffff";
  uint32_t nfgc = *fgc.val() & 0x00ffffff;
  snprintf(color, sizeof(color), "#%06X", nfgc);

  if (!DisplayManager::Instance()->xft_color_alloc_name(visual_ptr, colormap, color, &sel_fg)) {
    fprintf(stderr, "Couldn't allocate xft font color '%s'\n", color);
  }
  X::Instance()->flush();
}

void
cleanup ()
{
  for (const auto& font : fonts._fonts) {
    // replace with ResourceManager::Instance()->font_close(font);
    if (font.xft_ft) {
      XftFontClose (DisplayManager::Instance()->get_display(), font.xft_ft);
    }
    else {
      xcb_close_font(X::Instance()->get_connection(), font.ptr);
      free(font.width_lut);
    }
  }

  for (const auto& mon : monitors) {
    xcb_destroy_window(X::Instance()->get_connection(), mon._window);
    xcb_free_pixmap(X::Instance()->get_connection(), mon._pixmap);
  }

  DisplayManager::Instance()->xft_color_free(visual_ptr, colormap, &sel_fg);

  if (gc[GC_DRAW])
    xcb_free_gc(X::Instance()->get_connection(), gc[GC_DRAW]);
  if (gc[GC_CLEAR])
    xcb_free_gc(X::Instance()->get_connection(), gc[GC_CLEAR]);
  if (gc[GC_ATTR])
    xcb_free_gc(X::Instance()->get_connection(), gc[GC_ATTR]);
}

void
sighandle (int signal)
{
  if (signal == SIGINT || signal == SIGTERM)
    exit(EXIT_SUCCESS);
}

void
module_events() {
  char input[4096] = {0, };
  while (true) {
    // If connection is in error state, then it has been shut down.
    if (xcb_connection_has_error(X::Instance()->get_connection()))
      break;

    // a module has changed and the bar needs to be redrawn
    {
      std::mutex mutex;
      std::unique_lock<std::mutex> lock(mutex);
      module::condvar.wait(lock);

      std::stringstream ss;
      ss << "%{l} ";
      ss << modules.find("workspaces")->second->get();
      ss << " ";
      ss << modules.find("windows")->second->get();
      ss << "%{c}";
      ss << modules.find("clock")->second->get();
      ss << "%{r}";

      std::stringstream full_bar;
      std::string bar_str(ss.str());
      for (int i = 0; i < monitors._monitors.size(); ++i) {
        full_bar << "%{S" << i << "}" << bar_str;
      }
      std::string full_bar_str(full_bar.str());

      strncpy(input, full_bar_str.c_str(), full_bar_str.size());
      input[full_bar_str.size()] = '\0';
      parse(input);
    }

    for (const auto& mon : monitors) {
      xcb_copy_area(X::Instance()->get_connection(), mon._pixmap, mon._window, gc[GC_DRAW], 0, 0, 0, 0, mon._width, BAR_HEIGHT);
    }

    X::Instance()->flush();
  }
}

void
bar_events() {
  while (true) {
    bool redraw = false;

    // If connection is in error state, then it has been shut down.
    if (xcb_connection_has_error(X::Instance()->get_connection()))
      break;

    // handle bar related events
    for (xcb_generic_event_t *ev; (ev = xcb_wait_for_event(X::Instance()->get_connection())); free(ev)) {
      switch (ev->response_type & 0x7F) {
        case XCB_EXPOSE:
          redraw = reinterpret_cast<xcb_expose_event_t*>(ev)->count == 0;
          break;
        case XCB_BUTTON_PRESS:
          auto *press_ev = reinterpret_cast<xcb_button_press_event_t*>(ev);
          auto area = area_get(press_ev->event, press_ev->detail, press_ev->event_x);
          if (area) system(area->cmd);
          break;
      }
    }

    if (redraw) { // Copy our temporary pixmap onto the window
      for (const auto& mon : monitors) {
        xcb_copy_area(X::Instance()->get_connection(), mon._pixmap, mon._window, gc[GC_DRAW], 0, 0, 0, 0, mon._width, BAR_HEIGHT);
      }
    }

    X::Instance()->flush();
  }

}

int
main ()
{
  if (!XInitThreads()) {
    fprintf(stderr, "Failed to initialize threading for Xlib\n");
    exit(EXIT_FAILURE);
  }

  // Install the parachute!
  atexit(cleanup);
  signal(SIGINT, sighandle);
  signal(SIGTERM, sighandle);

  // Connect to the Xserver and initialize scr
  xconn();
    
  // Do the heavy lifting
  init();

  std::vector<std::thread> threads;
  for (const auto& mod : modules) {
    threads.emplace_back(std::ref(*mod.second));
  }
  threads.emplace_back(bar_events);
  threads.emplace_back(module_events);

  for (std::thread& thread : threads) {
    thread.join();
  }

  return EXIT_SUCCESS;
}
