#include "x.h"

#include <X11/Xatom.h>

X* X::instance = nullptr;

X::X() {
  display = XOpenDisplay(nullptr);
  if (!display) {
    fprintf (stderr, "Couldnt open display\n");
    exit(EXIT_FAILURE);
  }

  if ((connection = XGetXCBConnection(display)) == nullptr) {
    fprintf (stderr, "Couldnt connect to X\n");
    exit (EXIT_FAILURE);
  }

  set_event_queue_order(XCBOwnsEventQueue);

  if (xcb_connection_has_error(connection)) {
    fprintf(stderr, "X connection has error.\n");
    exit(EXIT_FAILURE);
  }

  if (!(database = xcb_xrm_database_from_default(connection))) {
    fprintf(stderr, "Could not connect to database\n");
    exit(EXIT_FAILURE);
  }
  screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
}

X::~X() {
  if (connection) {
    xcb_disconnect(connection);
  }
  if (database) {
    xcb_xrm_database_free(database);
  }
  delete instance;
}

X*
X::Instance() {
  if (!instance) {
    instance = new X();
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
X::update_gc ()
{
  /* xcb_change_gc(connection, gc[GC_DRAW], XCB_GC_FOREGROUND, fgc.val()); */
  /* xcb_change_gc(connection, gc[GC_CLEAR], XCB_GC_FOREGROUND, bgc.val()); */
  /* xcb_change_gc(connection, gc[GC_ATTR], XCB_GC_FOREGROUND, ugc.val()); */
  /* xft_color_free(visual_ptr, colormap, &sel_fg); */
  /* char color[] = "#ffffff"; */
  /* uint32_t nfgc = *fgc.val() & 0x00ffffff; */
  /* snprintf(color, sizeof(color), "#%06X", nfgc); */
  /* if (!xft_color_alloc_name(visual_ptr, colormap, color, &sel_fg)) { */
  /*   fprintf(stderr, "Couldn't allocate xft font color '%s'\n", color); */
  /* } */
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
X::xft_color_alloc_name(_Xconst Visual *visual, Colormap cmap,
                        _Xconst char *name, XftColor *result)
{
  return XftColorAllocName(display, visual, cmap, name, result);
}

void
X::xft_color_free(Visual *visual, Colormap cmap, XftColor *color) {
  XftColorFree(display, visual, cmap, color);
}

Visual *
X::xft_default_visual(int screen) {
  // TODO: is this even necessary?
  fprintf(stderr, "SCREEN IS %d\n", screen);
  return DefaultVisual(display, screen);
}

XftDraw *
X::xft_draw_create(Drawable drawable, Visual* visual, Colormap colormap) {
  return XftDrawCreate(display, drawable, visual, colormap);
}

XftFont *
X::xft_font_open_name(int screen, _Xconst char *name) {
  return XftFontOpenName(display, screen, name);
}

void
X::xft_font_close(XftFont *xft) {
  XftFontClose (display, xft);
}
