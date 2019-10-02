// vim:sw=2:ts=2:et:

/** TODO
 * - Convert global variables into Singletons.
 * - Modules should return pixmaps or a format that does not need to be parsed
 *   but is instead sent directly to the bar.
 * - Find more ergonomic way to reference singletons.
 * - Use static polymorphism with modules.
 */

#include "color.h"
#include "config.h"
#include "fonts.h"
#include "monitors.h"
#include "x.h"

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

static Fonts fonts;
static Monitors monitors;


void
update_gc ()
{
  xcb_change_gc(X::Instance()->get_connection(), gc[GC_DRAW], XCB_GC_FOREGROUND, fgc.val());
  xcb_change_gc(X::Instance()->get_connection(), gc[GC_CLEAR], XCB_GC_FOREGROUND, bgc.val());
  xcb_change_gc(X::Instance()->get_connection(), gc[GC_ATTR], XCB_GC_FOREGROUND, ugc.val());
  X::Instance()->xft_color_free(visual_ptr, colormap, &sel_fg);
  char color[] = "#ffffff";
  uint32_t nfgc = *fgc.val() & 0x00ffffff;
  snprintf(color, sizeof(color), "#%06X", nfgc);
  if (!X::Instance()->xft_color_alloc_name(visual_ptr, colormap, color, &sel_fg)) {
    fprintf(stderr, "Couldn't allocate xft font color '%s'\n", color);
  }
}

void
fill_rect (xcb_drawable_t d, xcb_gcontext_t _gc, int16_t x, int16_t y, uint16_t width, uint16_t height)
{
  xcb_rectangle_t rect = { x, y, width, height };
  xcb_poly_fill_rectangle(X::Instance()->get_connection(), d, _gc, 1, &rect);
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
  int ch_width = X::Instance()->xft_char_width(ch, cur_font->xft_ft);
  x = shift(mon, x, align, ch_width);

  int y = BAR_HEIGHT / 2 + cur_font->height / 2 - cur_font->descent + cur_font->offset;
  XftDrawString16 (xft_draw, &sel_fg, cur_font->xft_ft, x, y, &ch, 1);

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

  pos_x = 0;
  align = ALIGN_L;
  auto mon_itr = monitors.begin();

  areas.clear();

  for (const monitor_t& m : monitors)
    fill_rect(m._pixmap, gc[GC_CLEAR], 0, 0, m._width, BAR_HEIGHT);

  /* Create xft drawable */
  if (!(xft_draw = (X::Instance()->xft_draw_create(mon_itr->_pixmap, visual_ptr, colormap)))) {
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
            if (!(xft_draw = X::Instance()->xft_draw_create(mon_itr->_pixmap, visual_ptr , colormap ))) {
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
      xcb_randr_get_screen_resources_current(X::Instance()->get_connection(), X::Instance()->get_screen()->root), nullptr);

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

  monitors.init(rects, visual, bgc, colormap);
}

xcb_visualid_t
get_visual ()
{

  XVisualInfo xv; 
  xv.depth = 32;
  int result = 0;
  XVisualInfo* result_ptr = X::Instance()->get_visual_info(VisualDepthMask, &xv, &result);

  if (result > 0) {
    visual_ptr = result_ptr->visual;
    return result_ptr->visualid;
  }

  //Fallback
  visual_ptr = X::Instance()->xft_default_visual(scr_nbr);
  return X::Instance()->get_screen()->root_visual;
}

void
init ()
{
  /* Try to get a RGBA visual and build the colormap for that */
  visual = get_visual();
  colormap = xcb_generate_id(X::Instance()->get_connection());
  xcb_create_colormap(X::Instance()->get_connection(), XCB_COLORMAP_ALLOC_NONE, colormap, X::Instance()->get_screen()->root, visual);

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
  if (!qe_reply || !qe_reply->present) {
    fprintf(stderr, "Error with xcb_get_extension_data.\n");
    exit(EXIT_FAILURE);
  }
  get_randr_monitors();

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

  if (!X::Instance()->xft_color_alloc_name(visual_ptr, colormap, color, &sel_fg)) {
    fprintf(stderr, "Couldn't allocate xft font color '%s'\n", color);
  }
  X::Instance()->flush();
}

void
cleanup ()
{
  X::Instance()->xft_color_free(visual_ptr, colormap, &sel_fg);

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
