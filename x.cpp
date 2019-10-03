#include "x.h"

#include "monitors.h"

#include <xcb/randr.h>
#include <X11/Xatom.h>

X* X::instance = nullptr;

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

  //Fallback
  visual_ptr = xft_default_visual(0);
  return screen->root_visual;
}

X::X() {
  if (!XInitThreads()) {
    fprintf(stderr, "Failed to initialize threading for Xlib\n");
    exit(EXIT_FAILURE);
  }

  if (!(display = XOpenDisplay(nullptr))) {
    fprintf (stderr, "Couldnt open display\n");
    exit(EXIT_FAILURE);
  }

  if (!(connection = XGetXCBConnection(display))) {
    fprintf (stderr, "Couldnt connect to X\n");
    exit (EXIT_FAILURE);
  }
  if (xcb_connection_has_error(connection)) {
    fprintf(stderr, "X connection has error\n");
    exit(EXIT_FAILURE);
  }

  set_event_queue_order(XCBOwnsEventQueue);

  if (!(database = xcb_xrm_database_from_default(connection))) {
    fprintf(stderr, "Could not connect to database\n");
    exit(EXIT_FAILURE);
  }
  screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;

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

  // set_ewmh_atoms();
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

  delete instance;
}

void
X::init() {
  // for the love of God, remove the requirement to indirectly initialize
  // monitors from the X constructor... ugh...
  get_randr_monitors();

  // Create the gc for drawing
  gc[GC_DRAW] = xcb_generate_id(connection);
  xcb_create_gc(connection, gc[GC_DRAW], Monitors::Instance()->begin()->_pixmap, XCB_GC_FOREGROUND, fgc.val());

  gc[GC_CLEAR] = xcb_generate_id(connection);
  xcb_create_gc(connection, gc[GC_CLEAR], Monitors::Instance()->begin()->_pixmap, XCB_GC_FOREGROUND, bgc.val());

  gc[GC_ATTR] = xcb_generate_id(connection);
  xcb_create_gc(connection, gc[GC_ATTR], Monitors::Instance()->begin()->_pixmap, XCB_GC_FOREGROUND, ugc.val());
}

std::mutex _init_mutex;  // TODO: find a better way to do this.
X*
X::Instance() {
  // TODO: this is disgusting
  bool needs_init = false;
  {
    std::lock_guard<std::mutex> lock(_init_mutex);
    if (!instance) {
      instance = new X();
      needs_init = true;
    }
  }
  if (needs_init) {
    instance->init();
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

void X::copy_area(xcb_drawable_t src, xcb_drawable_t dst, int16_t src_x,
                  int16_t dst_x, uint16_t width)
{
  xcb_copy_area(connection, src, dst, gc[GC_DRAW], src_x, 0, dst_x, 0, width,
                BAR_HEIGHT);
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

void
X::get_randr_monitors () {
  xcb_randr_get_screen_resources_current_reply_t *rres_reply;
  xcb_randr_output_t *outputs;

  rres_reply = xcb_randr_get_screen_resources_current_reply(connection,
      xcb_randr_get_screen_resources_current(connection, screen->root), nullptr);

  if (!rres_reply) {
    fprintf(stderr, "Failed to get current randr screen resources\n");
    return;
  }

  int num = xcb_randr_get_screen_resources_current_outputs_length(rres_reply);
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

    oi_reply = xcb_randr_get_output_info_reply(connection,
        xcb_randr_get_output_info(connection, outputs[i], XCB_CURRENT_TIME),
        nullptr);

    // don't attach outputs that are disconnected or not attached to any CTRC
    if (!oi_reply || oi_reply->crtc == XCB_NONE || oi_reply->connection != XCB_RANDR_CONNECTION_CONNECTED) {
      free(oi_reply);
      continue;
    }

    ci_reply = xcb_randr_get_crtc_info_reply(connection,
        xcb_randr_get_crtc_info(connection, oi_reply->crtc, XCB_CURRENT_TIME), nullptr);

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

  Monitors::Instance()->init(rects, visual, bgc, colormap);
}

void
X::set_ewmh_atoms () {
  // TODO: This doesn't work yet for some reason
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
  std::transform(atom_names.begin(), atom_names.end(), atom_cookies.begin(), [this](auto name){
    return xcb_intern_atom(connection, 0, strlen(name), name);
  });

  for (int i = 0; i < atom_names.size(); i++) {
    atom_reply = xcb_intern_atom_reply(connection, atom_cookies[i], nullptr);
    if (!atom_reply)
      return;
    atom_list[i] = atom_reply->atom;
    free(atom_reply);
  }

  // Prepare the strut array
  for (const auto& mon : *Monitors::Instance()) {
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

    change_property(XCB_PROP_MODE_REPLACE, mon._window, atom_list[NET_WM_WINDOW_TYPE], XCB_ATOM_ATOM, 32, 1, &atom_list[NET_WM_WINDOW_TYPE_DOCK]);
    change_property(XCB_PROP_MODE_APPEND,  mon._window, atom_list[NET_WM_STATE], XCB_ATOM_ATOM, 32, 2, &atom_list[NET_WM_STATE_STICKY]);
    change_property(XCB_PROP_MODE_REPLACE, mon._window, atom_list[NET_WM_DESKTOP], XCB_ATOM_CARDINAL, 32, 1, (const uint32_t []) { 0u - 1u } );
    change_property(XCB_PROP_MODE_REPLACE, mon._window, atom_list[NET_WM_STRUT_PARTIAL], XCB_ATOM_CARDINAL, 32, 12, strut);
    change_property(XCB_PROP_MODE_REPLACE, mon._window, atom_list[NET_WM_STRUT], XCB_ATOM_CARDINAL, 32, 4, strut);
    change_property(XCB_PROP_MODE_REPLACE, mon._window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 3, "bar");
    change_property(XCB_PROP_MODE_REPLACE, mon._window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, 12, "lemonbar\0Bar");
  }
}


void
X::set_event_queue_order(enum XEventQueueOwner owner) {
  XSetEventQueueOwner(display, owner);
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
X::xft_char_width (uint16_t ch, XftFont *font) {
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
X::xft_font_open_name(int screen, _Xconst char *name) {
  return XftFontOpenName(display, screen, name);
}

void
X::xft_font_close(XftFont *xft) {
  XftFontClose (display, xft);
}
