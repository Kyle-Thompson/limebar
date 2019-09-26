// vim:sw=2:ts=2:et:

/** TODO
 * - Convert global variables into Singletons.
 * - Modules should return pixmaps or a format that does not need to be parsed
 *   but is instead sent directly to the bar.
 * - Find more ergonomic way to reference singletons.
 * - Add more functions into DisplayManager singleton. Too many raw calls
 *   happening here that should be members.
 */

#include "DisplayManager.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <poll.h>
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

struct font_t {
  xcb_font_t ptr;
  xcb_charinfo_t *width_lut;

  XftFont *xft_ft;

  int ascent;

  int descent, height, width;
  uint16_t char_max;
  uint16_t char_min;
  int offset;
};

struct monitor_t {
  int x, y, width;
  xcb_window_t window;
  xcb_pixmap_t pixmap;
};

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

// user configs
static constexpr bool TOPBAR = true;
static constexpr bool FORCE_DOCK = false;
static constexpr int BAR_WIDTH = 5760, BAR_HEIGHT = 20, BAR_X_OFFSET = 0, BAR_Y_OFFSET = 0;
static constexpr const char* WM_NAME = nullptr;
static constexpr std::string_view WM_CLASS = "limebar";
static constexpr int UNDERLINE_HEIGHT = 1;

// font name, y offset
static std::array<std::tuple<const char*, int>, 1> FONTS = {
  std::make_tuple("Gohu GohuFont", 0) };


static std::vector<monitor_t> monitors;

static std::mutex mutex;
static std::condition_variable condvar;

static xcb_connection_t *c;
static xcb_xrm_database_t *db;

static xcb_screen_t *scr;
static int scr_nbr = 0;

static xcb_gcontext_t gc[GC_MAX];
static xcb_visualid_t visual;
static Visual *visual_ptr;
static xcb_colormap_t colormap;


static std::array<font_t, FONTS.size()> fonts;
static int font_index = -1;

static uint32_t attrs = 0;
static rgba_t fgc, bgc, ugc;

static std::vector<area_t> areas;

static XftColor sel_fg;
static XftDraw *xft_draw;

//char width lookuptable
constexpr size_t MAX_WIDTHS {1 << 16};
static wchar_t xft_char[MAX_WIDTHS];
static char    xft_width[MAX_WIDTHS];

class module {
 public:
  module() {}
  virtual ~module() {}

  std::string get() {
    std::lock_guard<std::mutex> g(_mutex);
    return _str;
  }
  void operator()(int) {  // TODO: remove param
    update();
    while (true) {
      trigger();
      update();
    }
  }

 protected:
  void set(std::string str) {
    std::lock_guard<std::mutex> g(_mutex);
    _str = str;
    condvar.notify_one();
  }
  virtual void trigger() = 0;
  virtual void update()  = 0;
  std::mutex _mutex;
  std::string _str;
};

class mod_windows : public module {
 public:
  mod_windows() {
    conn = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(conn)) {
      fprintf(stderr, "Cannot X connection for workspaces daemon.\n");
      exit(EXIT_FAILURE);
    }

    const char *window = "_NET_ACTIVE_WINDOW";
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn,
        xcb_intern_atom(conn, 0, static_cast<uint16_t>(strlen(window)), window), nullptr);
    active_window = reply ? reply->atom : XCB_NONE;
    free(reply);

    uint32_t values = XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK, &values);
    xcb_flush(conn);

    const char *desktop = "_NET_CURRENT_DESKTOP";
    reply = xcb_intern_atom_reply(conn,
        xcb_intern_atom(conn, 0, static_cast<uint16_t>(strlen(desktop)), desktop), nullptr);
    current_desktop = reply ? reply->atom : XCB_NONE;
    free(reply);

    screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK, &values);
    xcb_flush(conn);
  }

  ~mod_windows() {
    xcb_disconnect(conn);
  }

 private:
  void trigger() {
    for (xcb_generic_event_t *ev = nullptr; (ev = xcb_wait_for_event(conn)); free(ev)) {
      if ((ev->response_type & 0x7F) == XCB_PROPERTY_NOTIFY) {
        auto atom = reinterpret_cast<xcb_property_notify_event_t *>(ev)->atom;
        if (atom == active_window || atom == current_desktop) {
          free(ev);
          return;
        }
      }
    }
  }
  void update() {
    std::stringstream ss;
    unsigned long client_list_size;
    unsigned long current_workspace = DisplayManager::Instance()->get_current_workspace();

    const Window current_window = [] {
      unsigned long size;
      char* prop = DisplayManager::Instance()->get_property(DisplayManager::Instance()->get_default_root_window(), XA_WINDOW, "_NET_ACTIVE_WINDOW", &size);
      Window ret = *((Window*)prop);
      free(prop);
      return ret;
    }();

    // TODO: how to capture windows that don't work here? (e.g. steam)
    Window* windows = DisplayManager::Instance()->get_client_list(&client_list_size);
    for (unsigned long i = 0; i < client_list_size / sizeof(Window); ++i) {
      unsigned long *workspace = (unsigned long *)DisplayManager::Instance()->get_property(windows[i],
          XA_CARDINAL, "_NET_WM_DESKTOP", nullptr);
      char* title_cstr = DisplayManager::Instance()->get_window_title(windows[i]);
      if (!title_cstr || current_workspace != *workspace) continue;
      std::string title(title_cstr);
      if (windows[i] == current_window) ss << "%{F#257fad}";  // TODO: replace with general xres accent color
      ss << title.substr(title.find_last_of(' ') + 1) << " ";
      if (windows[i] == current_window) ss << "%{F#7ea2b4}";
    }
    set(ss.str());
    free(windows);
  }

  xcb_connection_t* conn;
  xcb_atom_t current_desktop;
  xcb_atom_t active_window;
};

class mod_workspaces : public module {
 public:
  mod_workspaces() {
    conn = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(conn)) {
      fprintf(stderr, "Cannot X connection for workspaces daemon.\n");
      exit(EXIT_FAILURE);
    }

    const char *desktop = "_NET_CURRENT_DESKTOP";
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn,
        xcb_intern_atom(conn, 0, static_cast<uint16_t>(strlen(desktop)), desktop), nullptr);
    current_desktop = reply ? reply->atom : XCB_NONE;
    free(reply);

    uint32_t values = XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK, &values);
    xcb_flush(conn);
  }
  ~mod_workspaces() {}

 private:
  void trigger() {
    // TODO: this doesn't work if switching an empty workspace to an empty
    // workspace.
    for (xcb_generic_event_t *ev = nullptr; (ev = xcb_wait_for_event(conn)); free(ev)) {
      if ((ev->response_type & 0x7F) == XCB_PROPERTY_NOTIFY
          && reinterpret_cast<xcb_property_notify_event_t *>(ev)->atom == current_desktop) {
        free(ev);
        return;
      }
    }
  }

  void update() {
    unsigned long desktop_list_size = 0;
    Window root = DisplayManager::Instance()->get_default_root_window();

    unsigned long *num_desktops = (unsigned long *)DisplayManager::Instance()->get_property(root, XA_CARDINAL, "_NET_NUMBER_OF_DESKTOPS", NULL);

    unsigned long *cur_desktop = (unsigned long *)DisplayManager::Instance()->get_property(root, XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);

    char *list = DisplayManager::Instance()->get_property(root, DisplayManager::Instance()->get_intern_atom(), "_NET_DESKTOP_NAMES", &desktop_list_size);

    /* prepare the array of desktop names */
    char **names = (char **) malloc(*num_desktops * sizeof(char *));
    int id = 0;
    names[id++] = list;
    for (int i = 0; i < desktop_list_size; i++) {
      if (list[i] == '\0') {
        if (id >= *num_desktops) {
          break;
        }
        names[id++] = list + i + 1;
      }
    }

    std::stringstream ss;
    for (int i = 0; i < *num_desktops; ++i) {
      if (i == *cur_desktop) ss << "%{F#257fad}";
      ss << names[i] << " ";
      if (i == *cur_desktop) ss << "%{F#7ea2b4}";
    }
    set(ss.str());

    free(names);
    free(num_desktops);
    free(cur_desktop);
    free(list);
  }

  xcb_connection_t* conn;
  xcb_atom_t current_desktop;
};

class mod_clock : public module {
 public:
  mod_clock() {}
  ~mod_clock() {}

 private:
  void trigger() {
    std::this_thread::sleep_for(std::chrono::minutes(1));
  }

  void update() {
    time_t t = time(NULL);
    struct tm* local = localtime(&t);
    // TODO: optimize for the fixed size nature of this string.
    std::stringstream ss;
    // TODO: always show a 2 digit hour
    ss << "%{F#257fad}" << local->tm_hour << ':' << local->tm_min << "%{F#7ea2b4}"
       << " " << months[local->tm_mon] << " " << local->tm_mday;
    set(ss.str());
  }

  static constexpr std::array<const char*, 12> months {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", };
};


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
  xcb_change_gc(c, gc[GC_DRAW], XCB_GC_FOREGROUND, fgc.val());
  xcb_change_gc(c, gc[GC_CLEAR], XCB_GC_FOREGROUND, bgc.val());
  xcb_change_gc(c, gc[GC_ATTR], XCB_GC_FOREGROUND, ugc.val());
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
  xcb_poly_fill_rectangle(c, d, _gc, 1, &rect);
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
xft_char_width_slot (uint16_t ch)
{
  int slot = ch % MAX_WIDTHS;
  while (xft_char[slot] != 0 && xft_char[slot] != ch)
  {
    slot = (slot + 1) % MAX_WIDTHS;
  }
  return slot;
}

int
xft_char_width (uint16_t ch, font_t *cur_font)
{
  int slot = xft_char_width_slot(ch);
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
      xcb_copy_area(c, mon->pixmap, mon->pixmap, gc[GC_DRAW],
          mon->width / 2 - x / 2, 0,
          mon->width / 2 - (x + ch_width) / 2, 0,
          x, BAR_HEIGHT);
      x = mon->width / 2 - (x + ch_width) / 2 + x;
      break;
    case ALIGN_R:
      xcb_copy_area(c, mon->pixmap, mon->pixmap, gc[GC_DRAW],
          mon->width - x, 0,
          mon->width - x - ch_width, 0,
          x, BAR_HEIGHT);
      x = mon->width - ch_width;
      break;
  }

  /* Draw the background first */
  fill_rect(mon->pixmap, gc[GC_CLEAR], x, 0, ch_width, BAR_HEIGHT);
  return x;
}

void
draw_lines (monitor_t *mon, int x, int w)
{
  /* We can render both at the same time */
  if (attrs & ATTR_OVERL)
    fill_rect(mon->pixmap, gc[GC_ATTR], x, 0, w, UNDERLINE_HEIGHT);
  if (attrs & ATTR_UNDERL)
    fill_rect(mon->pixmap, gc[GC_ATTR], x, BAR_HEIGHT - UNDERLINE_HEIGHT, w, UNDERLINE_HEIGHT);
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

  int y = BAR_HEIGHT / 2 + cur_font->height / 2- cur_font->descent + fonts[font_index].offset;
  if (cur_font->xft_ft) {
    XftDrawString16 (xft_draw, &sel_fg, cur_font->xft_ft, x,y, &ch, 1);
  } else {
    /* xcb accepts string in UCS-2 BE, so swap */
    ch = (ch >> 8) | (ch << 8);

    // The coordinates here are those of the baseline
    xcb_poly_text_16_simple(c, mon->pixmap, gc[GC_DRAW],
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
      tmp.set((   *tmp.val() & 0xf00) * 0x1100
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
area_add (char *str, const char *optend, char **end, monitor_t *mon, const uint16_t x, const int8_t align, const uint8_t button)
{
  // A wild close area tag appeared!
  if (*str != ':') {
    *end = str;

    // Find most recent unclosed area.
    auto ritr = areas.rbegin();
    while (ritr != areas.rend() && !ritr->active)
      ++ritr;

    // Basic safety checks
    if (!ritr->cmd || ritr->align != align || ritr->window != mon->window) {
      fprintf(stderr, "Invalid geometry for the clickable area\n");
      return false;
    }

    const int size = x - ritr->begin;
    switch (align) {
      case ALIGN_L:
        ritr->end = x;
        break;
      case ALIGN_C:
        ritr->begin = mon->width / 2 - size / 2 + ritr->begin / 2;
        ritr->end = ritr->begin + size;
        break;
      case ALIGN_R:
        // The newest is the rightmost one
        ritr->begin = mon->width - size;
        ritr->end = mon->width;
        break;
    }

    ritr->active = false;
    return true;
  }

  // Found the closing : and check if it's just an escaped one
  char *trail;
  for (trail = strchr(++str, ':'); trail && trail[-1] == '\\'; trail = strchr(trail + 1, ':'))
    ;

  // Find the trailing : and make sure it's within the formatting block, also reject empty commands
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
    .window = mon->window,
    .cmd = str,
  });

  *end = trail + 1;

  return true;
}

bool
font_has_glyph (font_t *font, const uint16_t c)
{
  if (font->xft_ft)
    return DisplayManager::Instance()->xft_char_exists(font->xft_ft, (FcChar32) c);

  if (c < font->char_min || c > font->char_max)
    return false;

  if (font->width_lut && font->width_lut[c - font->char_min].character_width == 0)
    return false;

  return true;
}

font_t *
select_drawable_font (const uint16_t c)
{
  // If the user has specified a font to use, try that first.
  if (font_index != -1 && font_has_glyph(&fonts[font_index], c)) {
    return &fonts[font_index];
  }

  // If the end is reached without finding an appropriate font, return nullptr.
  // If the font can draw the character, return it.
  for (auto& font : fonts) {
    if (font_has_glyph(&font, c)) {
      return &font;
    }
  }
  return nullptr;
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
    fill_rect(m.pixmap, gc[GC_CLEAR], 0, 0, m.width, BAR_HEIGHT);

  /* Create xft drawable */
  if (!(xft_draw = (DisplayManager::Instance()->xft_draw_create(mon_itr->pixmap, visual_ptr, colormap)))) {
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
            if (!(xft_draw = DisplayManager::Instance()->xft_draw_create(mon_itr->pixmap, visual_ptr , colormap ))) {
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
            area_shift(mon_itr->window, align, w);
            break;

          case 'T':
            if (*p == '-') { //Reset to automatic font selection
              font_index = -1;
              p++;
              break;
            } else if (isdigit(*p)) {
              font_index = (int)strtoul(p, &ep, 10);
              // User-specified 'font_index' ∊ (0,font_count]
              // Otherwise just fallback to the automatic font selection
              if (font_index < 0 || font_index > FONTS.size())
                font_index = -1;
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

      cur_font = select_drawable_font(ucs);
      if (!cur_font)
        continue;

      if(cur_font->ptr)
        xcb_change_gc(c, gc[GC_DRAW] , XCB_GC_FONT, &cur_font->ptr);
      int w = draw_char(&*mon_itr, cur_font, pos_x, align, ucs);

      pos_x += w;
      area_shift(mon_itr->window, align, w);
    }
  }
  XftDrawDestroy (xft_draw);
}

font_t
font_load (const char *pattern, int offset)
{
  xcb_query_font_cookie_t queryreq;
  xcb_query_font_reply_t *font_info;
  xcb_void_cookie_t cookie;
  xcb_font_t font;

  font = xcb_generate_id(c);

  font_t ret;

  cookie = xcb_open_font_checked(c, font, strlen(pattern), pattern);
  if (!xcb_request_check (c, cookie)) {
    queryreq = xcb_query_font(c, font);
    font_info = xcb_query_font_reply(c, queryreq, nullptr);

    ret.xft_ft = nullptr;
    ret.ptr = font;
    ret.descent = font_info->font_descent;
    ret.height = font_info->font_ascent + font_info->font_descent;
    ret.width = font_info->max_bounds.character_width;
    ret.char_max = font_info->max_byte1 << 8 | font_info->max_char_or_byte2;
    ret.char_min = font_info->min_byte1 << 8 | font_info->min_char_or_byte2;
    ret.offset = offset;
    // Copy over the width lut as it's part of font_info
    int lut_size = sizeof(xcb_charinfo_t) * xcb_query_font_char_infos_length(font_info);
    if (lut_size) {
      ret.width_lut = (xcb_charinfo_t *) malloc(lut_size);
      memcpy(ret.width_lut, xcb_query_font_char_infos(font_info), lut_size);
    }
    free(font_info);
  } else if ((ret.xft_ft = DisplayManager::Instance()->xft_font_open_name(scr_nbr, pattern))) {
    ret.ptr = 0;
    ret.ascent = ret.xft_ft->ascent;
    ret.descent = ret.xft_ft->descent;
    ret.height = ret.ascent + ret.descent;
    ret.offset = offset;
  } else {
    fprintf(stderr, "Could not load font %s\n", pattern);
    exit(EXIT_FAILURE);
  }

  return ret;
}


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

void
set_ewmh_atoms ()
{
  constexpr size_t size = 8;
  constexpr std::array<const char *, size> atom_names {
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
    return xcb_intern_atom(c, 0, strlen(name), name);
  });

  for (int i = 0; i < atom_names.size(); i++) {
    atom_reply = xcb_intern_atom_reply(c, atom_cookies[i], nullptr);
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
      strut[8] = mon.x;
      strut[9] = mon.x + mon.width;
    } else {
      strut[3]  = BAR_HEIGHT;
      strut[10] = mon.x;
      strut[11] = mon.x + mon.width;
    }

    xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon.window, atom_list[NET_WM_WINDOW_TYPE], XCB_ATOM_ATOM, 32, 1, &atom_list[NET_WM_WINDOW_TYPE_DOCK]);
    xcb_change_property(c, XCB_PROP_MODE_APPEND,  mon.window, atom_list[NET_WM_STATE], XCB_ATOM_ATOM, 32, 2, &atom_list[NET_WM_STATE_STICKY]);
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon.window, atom_list[NET_WM_DESKTOP], XCB_ATOM_CARDINAL, 32, 1, (const uint32_t []) { 0u - 1u } );
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon.window, atom_list[NET_WM_STRUT_PARTIAL], XCB_ATOM_CARDINAL, 32, 12, strut);
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon.window, atom_list[NET_WM_STRUT], XCB_ATOM_CARDINAL, 32, 4, strut);
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon.window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 3, "bar");
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon.window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, 12, "lemonbar\0Bar");
  }
}

monitor_t
monitor_new (int x, int y, int width, int height)
{
  monitor_t ret;

  ret.x = x;
  ret.y = (TOPBAR ? BAR_Y_OFFSET : height - BAR_HEIGHT - BAR_Y_OFFSET) + y;
  ret.width = width;
  ret.window = xcb_generate_id(c);
  int depth = (visual == scr->root_visual) ? XCB_COPY_FROM_PARENT : 32;
  const uint32_t mask[] { *bgc.val(), *bgc.val(), FORCE_DOCK,
    XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE, colormap };
  xcb_create_window(c, depth, ret.window, scr->root,
      ret.x, ret.y, width, BAR_HEIGHT, 0,
      XCB_WINDOW_CLASS_INPUT_OUTPUT, visual,
      XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP,
      mask);

  ret.pixmap = xcb_generate_id(c);
  xcb_create_pixmap(c, depth, ret.pixmap, ret.window, width, BAR_HEIGHT);

  return ret;
}

void
monitor_create_chain (std::vector<xcb_rectangle_t>& rects)
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
      monitors.emplace_back(monitor_new(
          rect.x + left,
          rect.y,
          std::min(width, rect.width - left),
          rect.height));

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

void
get_randr_monitors ()
{
  xcb_randr_get_screen_resources_current_reply_t *rres_reply;
  xcb_randr_output_t *outputs;
  int num;

  rres_reply = xcb_randr_get_screen_resources_current_reply(c,
      xcb_randr_get_screen_resources_current(c, scr->root), nullptr);

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

    oi_reply = xcb_randr_get_output_info_reply(c, xcb_randr_get_output_info(c, outputs[i], XCB_CURRENT_TIME), nullptr);

    // don't attach outputs that are disconnected or not attached to any CTRC
    if (!oi_reply || oi_reply->crtc == XCB_NONE || oi_reply->connection != XCB_RANDR_CONNECTION_CONNECTED) {
      free(oi_reply);
      continue;
    }

    ci_reply = xcb_randr_get_crtc_info_reply(c,
        xcb_randr_get_crtc_info(c, oi_reply->crtc, XCB_CURRENT_TIME), nullptr);

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

  monitor_create_chain(rects);
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
  if ((c = DisplayManager::Instance()->get_xcb_connection()) == nullptr) {
    fprintf (stderr, "Couldnt connect to X\n");
    exit (EXIT_FAILURE);
  }

  DisplayManager::Instance()->set_event_queue_order(XCBOwnsEventQueue);

  if (xcb_connection_has_error(c)) {
    fprintf(stderr, "Couldn't connect to X\n");
    exit(EXIT_FAILURE);
  }

  /* Grab infos from the first screen */
  scr = xcb_setup_roots_iterator(xcb_get_setup(c)).data;

  /* Try to get a RGBA visual and build the colormap for that */
  visual = get_visual();
  colormap = xcb_generate_id(c);
  xcb_create_colormap(c, XCB_COLORMAP_ALLOC_NONE, colormap, scr->root, visual);
}

void
init ()
{
  // init fonts
  std::transform(FONTS.begin(), FONTS.end(), fonts.begin(),
      [](const auto& f){
        const auto& [font, offset] = f;
        return font_load(font, offset);
      });

  // To make the alignment uniform, find maximum height
  const int maxh = std::max_element(fonts.begin(), fonts.end(),
      [](const font_t& l, const font_t& r){ return l.height < r.height; })->height;

  // Set maximum height to all fonts
  for (auto& font : fonts)
    font.height = maxh;

  // connect to resource db
  db = xcb_xrm_database_from_default(c);

  if (!db) {
    fprintf(stderr, "Could not connect to database\n");
    exit(EXIT_FAILURE);
  }
  char *val;
  xcb_xrm_resource_get_string(db, "background", nullptr, &val);
  bgc = parse_color(val, nullptr);
  xcb_xrm_resource_get_string(db, "foreground", nullptr, &val);
  ugc = fgc = parse_color(val, nullptr);

  // Generate a list of screens
  const xcb_query_extension_reply_t *qe_reply;

  // Check if RandR is present
  qe_reply = xcb_get_extension_data(c, &xcb_randr_id);

  if (qe_reply && qe_reply->present) {
    get_randr_monitors();
  }

  if (monitors.empty()) {
    // Check the geometry
    if (BAR_X_OFFSET + BAR_WIDTH > scr->width_in_pixels || BAR_Y_OFFSET + BAR_HEIGHT > scr->height_in_pixels) {
      fprintf(stderr, "The geometry specified doesn't fit the screen!\n");
      exit(EXIT_FAILURE);
    }

    // If no RandR outputs, fall back to using whole screen
    monitors.emplace_back(monitor_new(0, 0, BAR_WIDTH, scr->height_in_pixels));
  }

  // For WM that support EWMH atoms
  set_ewmh_atoms();

  // Create the gc for drawing
  gc[GC_DRAW] = xcb_generate_id(c);
  xcb_create_gc(c, gc[GC_DRAW], monitors.begin()->pixmap, XCB_GC_FOREGROUND, fgc.val());

  gc[GC_CLEAR] = xcb_generate_id(c);
  xcb_create_gc(c, gc[GC_CLEAR], monitors.begin()->pixmap, XCB_GC_FOREGROUND, bgc.val());

  gc[GC_ATTR] = xcb_generate_id(c);
  xcb_create_gc(c, gc[GC_ATTR], monitors.begin()->pixmap, XCB_GC_FOREGROUND, ugc.val());

  // Make the bar visible and clear the pixmap
  for (const auto& mon : monitors) {
    fill_rect(mon.pixmap, gc[GC_CLEAR], 0, 0, mon.width, BAR_HEIGHT);
    xcb_map_window(c, mon.window);

    // Make sure that the window really gets in the place it's supposed to be
    // Some WM such as Openbox need this
    const uint32_t xy[] { static_cast<uint32_t>(mon.x), static_cast<uint32_t>(mon.y) };
    xcb_configure_window(c, mon.window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, xy);

    // Set the WM_NAME atom to the user specified value
    if constexpr (WM_NAME != nullptr)
      xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon.window,
          XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(WM_NAME), WM_NAME);

    // set the WM_CLASS atom instance to the executable name
    if (WM_CLASS.size()) {
      constexpr int size = WM_CLASS.size() + 6;
      char wm_class[size] = {0};

      // WM_CLASS is nullbyte seperated: WM_CLASS + "\0Bar\0"
      strncpy(wm_class, WM_CLASS.data(), WM_CLASS.size());
      strcpy(wm_class + WM_CLASS.size(), "\0Bar");

      xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon.window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, size, wm_class);
    }
  }

  char color[] = "#ffffff";
  uint32_t nfgc = *fgc.val() & 0x00ffffff;
  snprintf(color, sizeof(color), "#%06X", nfgc);

  if (!DisplayManager::Instance()->xft_color_alloc_name(visual_ptr, colormap, color, &sel_fg)) {
    fprintf(stderr, "Couldn't allocate xft font color '%s'\n", color);
  }
  xcb_flush(c);
}

void
cleanup ()
{
  for (const auto& font : fonts) {
    // replace with ResourceManager::Instance()->font_close(font);
    if (font.xft_ft) {
      XftFontClose (DisplayManager::Instance()->get_display(), font.xft_ft);
    }
    else {
      xcb_close_font(c, font.ptr);
      free(font.width_lut);
    }
  }

  for (const auto& mon : monitors) {
    xcb_destroy_window(c, mon.window);
    xcb_free_pixmap(c, mon.pixmap);
  }

  DisplayManager::Instance()->xft_color_free(visual_ptr, colormap, &sel_fg);

  if (gc[GC_DRAW])
    xcb_free_gc(c, gc[GC_DRAW]);
  if (gc[GC_CLEAR])
    xcb_free_gc(c, gc[GC_CLEAR]);
  if (gc[GC_ATTR])
    xcb_free_gc(c, gc[GC_ATTR]);
  if (c)
    xcb_disconnect(c);
  if (db)
    xcb_xrm_database_free(db);
}

void
sighandle (int signal)
{
  if (signal == SIGINT || signal == SIGTERM)
    exit(EXIT_SUCCESS);
}

void
main_loop() {
  char input[4096] = {0, };

  while (true) {
    bool redraw = false;

    // If connection is in error state, then it has been shut down.
    if (xcb_connection_has_error(c))
      break;

    // a module has changed and the bar needs to be redrawn
    // TODO: also check for click events while waiting. currently bar events
    // won't get seen since we're stuck here waiting for this condvar. Maybe
    // separate into two different threads with a condvar of its own to sync.
    {
      std::unique_lock<std::mutex> lock(mutex);
      condvar.wait(lock);

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
      for (int i = 0; i < monitors.size(); ++i) {
        full_bar << "%{S" << i << "}" << bar_str;
      }
      std::string full_bar_str(full_bar.str());

      strncpy(input, full_bar_str.c_str(), full_bar_str.size());
      input[full_bar_str.size()] = '\0';
      parse(input);
      redraw = true;
    }

    // handle bar related events
    for (xcb_generic_event_t *ev; (ev = xcb_poll_for_event(c)); free(ev)) {
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
        xcb_copy_area(c, mon.pixmap, mon.window, gc[GC_DRAW], 0, 0, 0, 0, mon.width, BAR_HEIGHT);
      }
    }

    xcb_flush(c);
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

  // Prevent fgets to block
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

  std::vector<std::thread> threads;
  for (const auto& mod : modules) {
    // TODO: find way to clean up threads
    threads.emplace_back(std::ref(*mod.second), 0);
  }

  main_loop();

  return EXIT_SUCCESS;
}
