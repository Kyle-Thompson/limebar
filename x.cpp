#include "x.h"

#include "config.h"

#include <X11/Xlib-xcb.h>
#include <xcb/randr.h>
#include <X11/Xatom.h>
#include <xcb/xcb_xrm.h>

xcb_visualid_t
X::get_visual () {
  XVisualInfo xv; 
  xv.depth = 32;
  int result = 0;
  XVisualInfo* result_ptr = get_visual_info(VisualDepthMask, &xv, &result);

  if (result > 0) {
    visual_ptr = result_ptr->visual;
    return result_ptr->visualid;
  }

  // Fallback
  visual_ptr = xft_default_visual(0);
  return screen->root_visual;
}

X::X()
  : display(XOpenDisplay(nullptr))
  , connection(XGetXCBConnection(display))
  , database(xcb_xrm_database_from_default(connection))
  , screen(xcb_setup_roots_iterator(xcb_get_setup(connection)).data)
{
  if (!display) {
    fprintf (stderr, "Couldnt open display\n");
    exit(EXIT_FAILURE);
  }

  if (!connection) {
    fprintf (stderr, "Couldn't connect to X\n");
    exit (EXIT_FAILURE);
  }
  if (connection_has_error()) {
    fprintf(stderr, "X connection has error\n");
    exit(EXIT_FAILURE);
  }

  set_event_queue_order(XCBOwnsEventQueue);

  if (!database) {
    fprintf(stderr, "Could not connect to database\n");
    exit(EXIT_FAILURE);
  }

  /* Try to get a RGBA visual and build the colormap for that */
  visual = get_visual();
  colormap = xcb_generate_id(connection);
  xcb_create_colormap(connection, XCB_COLORMAP_ALLOC_NONE, colormap, screen->root, visual);

  char *val;
  get_string_resource("background", &val);
  bgc = rgba_t::parse(val, nullptr);
  get_string_resource("foreground", &val);
  ugc = fgc = rgba_t::parse(val, nullptr);

  // Generate a list of screens
  const xcb_query_extension_reply_t *qe_reply = xcb_get_extension_data(connection, &xcb_randr_id);
  if (!qe_reply || !qe_reply->present) {
    // Check if RandR is present
    fprintf(stderr, "Error with xcb_get_extension_data.\n");
    exit(EXIT_FAILURE);
  }

  gc[GC_DRAW] = generate_id();
  gc[GC_CLEAR] = generate_id();
  gc[GC_ATTR] = generate_id();

  /* char color[] = "#ffffff"; */
  /* uint32_t nfgc = *fgc.val() & 0x00ffffff; */
  /* snprintf(color, sizeof(color), "#%06X", nfgc); */
  /* if (!xft_color_alloc_name(color)) { */
  /*   fprintf(stderr, "Couldn't allocate xft font color '%s'\n", color); */
  /* } */
}

X::~X() {
  if (gc[GC_DRAW])
    xcb_free_gc(connection, gc[GC_DRAW]);
  if (gc[GC_CLEAR])
    xcb_free_gc(connection, gc[GC_CLEAR]);
  if (gc[GC_ATTR])
    xcb_free_gc(connection, gc[GC_ATTR]);

  if (connection)
    xcb_disconnect(connection);
  if (database)
    xcb_xrm_database_free(database);

  xft_color_free();
}

X&
X::Instance() {
  static X instance;
  static bool temp = true;
  if (temp) {
    instance.fonts.init(&instance);
    temp = false;
  }
  return instance;
}


void
X::get_string_resource(const char* query, char **out) {
  xcb_xrm_resource_get_string(database, query, nullptr, out);
}

void
X::change_property(uint8_t mode, xcb_window_t window, xcb_atom_t property,
    xcb_atom_t type, uint8_t format, uint32_t data_len, const void *data)
{
  xcb_change_property(connection, mode, window, property, type, format, data_len, data);
}

void
X::update_gc () {
  xcb_change_gc(connection, gc[GC_DRAW], XCB_GC_FOREGROUND, fgc.val());
  xcb_change_gc(connection, gc[GC_CLEAR], XCB_GC_FOREGROUND, bgc.val());
  xcb_change_gc(connection, gc[GC_ATTR], XCB_GC_FOREGROUND, ugc.val());
  xft_color_free();
  char color[] = "#ffffff";
  uint32_t nfgc = *fgc.val() & 0x00ffffff;
  snprintf(color, sizeof(color), "#%06X", nfgc);
  if (!xft_color_alloc_name(color)) {
    fprintf(stderr, "Couldn't allocate xft font color '%s'\n", color);
  }
}

void
X::copy_area(xcb_drawable_t src, xcb_drawable_t dst, int16_t src_x,
             int16_t dst_x, uint16_t width)
{
  xcb_copy_area(connection, src, dst, gc[GC_DRAW], src_x, 0, dst_x, 0, width,
                BAR_HEIGHT);
}

void
X::create_gc(xcb_pixmap_t pixmap) {
  xcb_create_gc(connection, gc[GC_DRAW],  pixmap, XCB_GC_FOREGROUND, fgc.val());
  xcb_create_gc(connection, gc[GC_CLEAR], pixmap, XCB_GC_FOREGROUND, bgc.val());
  xcb_create_gc(connection, gc[GC_ATTR],  pixmap, XCB_GC_FOREGROUND, ugc.val());
}

void
X::create_pixmap(xcb_pixmap_t pid, xcb_drawable_t drawable, uint16_t width)
{
  xcb_create_pixmap(connection, get_depth(), pid, drawable, width, BAR_HEIGHT);
}

void
X::free_pixmap(xcb_pixmap_t pixmap) {
  xcb_free_pixmap(connection, pixmap);
}

void
X::create_window(xcb_window_t wid,
    int16_t x, int16_t y, uint16_t width, uint16_t _class,
    xcb_visualid_t visual, uint32_t value_mask, const void *value_list)
{
  xcb_create_window(connection, get_depth(), wid, screen->root, x, y, width,
      BAR_HEIGHT, 0, _class, visual, value_mask, value_list);
}

void
X::destroy_window(xcb_window_t window) {
  xcb_destroy_window(connection, window);
}

void
X::configure_window(xcb_window_t window, uint16_t mask, const void *list) {
  xcb_configure_window(connection, window, mask, list);
}

void
X::map_window(xcb_window_t window) {
  xcb_map_window(connection, window);
}

xcb_generic_event_t*
X::wait_for_event() {
  return xcb_wait_for_event(connection);
}

void
X::fill_rect (xcb_drawable_t d, uint32_t gc_index, int16_t x, int16_t y,
           uint16_t width, uint16_t height)
{
  xcb_rectangle_t rect = { x, y, width, height };
  xcb_poly_fill_rectangle(connection, d, gc[gc_index], 1, &rect);
}

Window
X::get_default_root_window() {
  return DefaultRootWindow(display);
}

Atom
X::get_intern_atom() {
  return XInternAtom(display, "UTF8_STRING", 0);
}

XVisualInfo *
X::get_visual_info(long vinfo_mask, XVisualInfo *vinfo_template,
                   int *nitems_return)
{
  return XGetVisualInfo(display, vinfo_mask, vinfo_template, nitems_return);
}

xcb_intern_atom_cookie_t
X::get_atom_by_name(const char* name) {
  return xcb_intern_atom(connection, 0, strlen(name), name);
}

xcb_intern_atom_reply_t*
X::get_intern_atom_reply(const char *name) {
  return xcb_intern_atom_reply(connection, get_atom_by_name(name), nullptr);
}

void
X::set_event_queue_order(enum XEventQueueOwner owner) {
  XSetEventQueueOwner(display, owner);
}

bool
X::connection_has_error() {
  return xcb_connection_has_error(connection);
}

// TODO: return string
char *
X::get_property(Window win, Atom xa_prop_type, const char *prop_name,
                unsigned long *size)
{
  Atom xa_prop_name;
  Atom xa_ret_type;
  int ret_format;
  unsigned long ret_nitems;
  unsigned long ret_bytes_after;
  unsigned long tmp_size;
  unsigned char *ret_prop;
  char *ret;

  xa_prop_name = XInternAtom(display, prop_name, False);

  if (XGetWindowProperty(display, win, xa_prop_name, 0, 1024, False,
      xa_prop_type, &xa_ret_type, &ret_format,
      &ret_nitems, &ret_bytes_after, &ret_prop) != Success) {
    return nullptr;
  }

  if (xa_ret_type != xa_prop_type) {
    XFree(ret_prop);
    return nullptr;
  }

  /* null terminate the result to make string handling easier */
  tmp_size = (ret_format / (32 / sizeof(long))) * ret_nitems;
  ret = (char *) malloc(tmp_size + 1);
  memcpy(ret, ret_prop, tmp_size);
  ret[tmp_size] = '\0';

  if (size) {
    *size = tmp_size;
  }

  XFree(ret_prop);
  return ret;
}


// TODO: return string
char *
X::get_window_title(Window win) {
  char *title_utf8 = nullptr;
  char *wm_name = get_property(win, XA_STRING, "WM_NAME", NULL);
  char *net_wm_name = get_property(win,
      XInternAtom(display, "UTF8_STRING", False), "_NET_WM_NAME", NULL);

  if (net_wm_name) {
    title_utf8 = strdup(net_wm_name);
  }

  free(wm_name);
  free(net_wm_name);

  return title_utf8;
}


Window *
X::get_client_list(unsigned long *size) {
  Window *client_list;

  if ((client_list = (Window *)get_property(DefaultRootWindow(display),
      XA_WINDOW, "_NET_CLIENT_LIST", size)) == NULL) {
    if ((client_list = (Window *)get_property(DefaultRootWindow(display),
        XA_CARDINAL, "_WIN_CLIENT_LIST", size)) == NULL) {
      fprintf(stderr, "Cannot get client list properties. \n"
          "(_NET_CLIENT_LIST or _WIN_CLIENT_LIST)\n");
      return NULL;
    }
  }

  return client_list;
}


unsigned long
X::get_current_workspace() {
  unsigned long *cur_desktop = NULL;
  Window root = DefaultRootWindow(display);
  if (! (cur_desktop = (unsigned long *)get_property(root,
      XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL))) {
    if (! (cur_desktop = (unsigned long *)get_property(root,
        XA_CARDINAL, "_WIN_WORKSPACE", NULL))) {
      fprintf(stderr, "Cannot get current desktop properties. "
          "(_NET_CURRENT_DESKTOP or _WIN_WORKSPACE property)\n");
      free(cur_desktop);
      exit(EXIT_FAILURE);
    }
  }
  unsigned long ret = *cur_desktop;
  free(cur_desktop);
  return ret;
}


// XFT functions

bool
X::xft_char_exists(XftFont *pub, FcChar32 ucs4) {
  return XftCharExists(display, pub, ucs4);
}

FT_UInt
X::xft_char_index(XftFont *pub, FcChar32 ucs4) {
  return XftCharIndex(display, pub, ucs4);
}

int
X::xft_char_width (uint16_t ch) {
  static bool temp_fix = true;
  if (temp_fix) {
    fonts.init(this);
    temp_fix = false;
  }

  XftFont *font = fonts.drawable_font(ch).xft_ft;
  const int slot = [this](uint16_t ch) {
    int slot = ch % MAX_WIDTHS;
    while (xft_char[slot] != 0 && xft_char[slot] != ch) {
      slot = (slot + 1) % MAX_WIDTHS;
    }
    return slot;
  }(ch);

  if (!xft_char[slot]) {
    XGlyphInfo gi;
    FT_UInt glyph = XftCharIndex(display, font, (FcChar32) ch);
    XftFontLoadGlyphs(display, font, FcFalse, &glyph, 1);
    XftGlyphExtents(display, font, &glyph, 1, &gi);
    XftFontUnloadGlyphs(display, font, &glyph, 1);
    xft_char[slot] = ch;
    xft_width[slot] = gi.xOff;
    return gi.xOff;
  } else if (xft_char[slot] == ch)
    return xft_width[slot];
  else
    return 0;
}

bool
X::xft_color_alloc_name(_Xconst char *name)
{
  return XftColorAllocName(display, visual_ptr, colormap, name, &sel_fg);
}

void
X::xft_color_free() {
  XftColorFree(display, visual_ptr, colormap, &sel_fg);
}

Visual *
X::xft_default_visual(int screen) {
  // TODO: is this even necessary?
  fprintf(stderr, "SCREEN IS %d\n", screen);
  return DefaultVisual(display, screen);
}

XftDraw *
X::xft_draw_create(Drawable drawable) {
  return XftDrawCreate(display, drawable, visual_ptr, colormap);
}

XftFont *
X::xft_font_open_name(_Xconst char *name) {
  return XftFontOpenName(display, 0, name);
}

void
X::xft_draw_string_16(XftFont* xft_ft, int x, int y, _Xconst FcChar16 *str,
                      int len)
{
  XftDrawString16(xft_draw, &sel_fg, xft_ft, x, y, str, len);
}

void
X::draw_ucs2_string(const std::vector<uint16_t>& str, size_t x) {
  // TODO: currently the proper font for this character needs to be found twice.
  // This can definitely be optimized.
  // Also, group consecutive characters of the same font so they can be printed
  // in bulk.

  for (auto ch : str) {
    auto& font = fonts.drawable_font(ch);
    const int y = BAR_HEIGHT / 2 + font.height / 2
                  - font.descent + font.offset;
    xft_draw_string_16(font.xft_ft, x, y, &ch, 1);
    x += xft_char_width(ch);
  }

}

void
X::xft_font_close(XftFont *xft) {
  XftFontClose (display, xft);
}

uint8_t
X::get_depth() {
  return (visual == screen->root_visual) ? XCB_COPY_FROM_PARENT : 32;
}