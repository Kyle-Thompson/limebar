#include "x.h"

#include "config.h"

#include <X11/Xlib-xcb.h>
#include <X11/Xutil.h>
#include <bits/stdint-intn.h>
#include <bits/stdint-uintn.h>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
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
  // TODO: when is this even used?
  visual_ptr = xft_default_visual(0);
  return screen->root_visual;
}

X::X()
{
  if (!(display = XOpenDisplay(nullptr))) {
    std::cerr << "Couldnt open display\n";
    exit(EXIT_FAILURE);
  }

  if (!(connection = XGetXCBConnection(display)) || connection_has_error()) {
    std::cerr << "Couldn't connect to X\n";
    exit (EXIT_FAILURE);
  }

  set_event_queue_order(XCBOwnsEventQueue);

  if (!(database = xcb_xrm_database_from_default(connection))) {
    std::cerr << "Could not connect to database\n";
    exit(EXIT_FAILURE);
  }

  screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
  /* Try to get a RGBA visual and build the colormap for that */
  visual = get_visual();
  colormap = xcb_generate_id(connection);
  xcb_create_colormap(connection, XCB_COLORMAP_ALLOC_NONE, colormap, screen->root, visual);

  char *val;
  get_string_resource("background", &val);
  bgc = rgba_t::parse(val);
  get_string_resource("foreground", &val);
  ugc = fgc = rgba_t::parse(val);
  accent = rgba_t::parse("#257fad");

  gc[GC_DRAW]   = generate_id();
  gc[GC_ACCENT] = generate_id();
  gc[GC_CLEAR]  = generate_id();
  gc[GC_ATTR]   = generate_id();
}

X::~X() {
  if (gc[GC_DRAW]) xcb_free_gc(connection, gc[GC_DRAW]);
  if (gc[GC_ACCENT]) xcb_free_gc(connection, gc[GC_ACCENT]);
  if (gc[GC_CLEAR]) xcb_free_gc(connection, gc[GC_CLEAR]);
  if (gc[GC_ATTR]) xcb_free_gc(connection, gc[GC_ATTR]);

  if (connection) xcb_disconnect(connection);
  if (database) xcb_xrm_database_free(database);

  xft_color_free(&fg_color);
  xft_color_free(&acc_color);
}

std::mutex _m;

X&
X::Instance() {
  static X instance;
  static bool temp = true;
  std::unique_lock lock{_m};  // TODO please god clean this function
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
X::update_gc() {
  xcb_change_gc(connection, gc[GC_DRAW],   XCB_GC_FOREGROUND, fgc.val());
  xcb_change_gc(connection, gc[GC_ACCENT], XCB_GC_FOREGROUND, accent.val());
  xcb_change_gc(connection, gc[GC_CLEAR],  XCB_GC_FOREGROUND, bgc.val());
  xcb_change_gc(connection, gc[GC_ATTR],   XCB_GC_FOREGROUND, ugc.val());

  // TODO: can't we just do this once initially?
  xft_color_free(&acc_color);
  if (!XftColorAllocName(display, visual_ptr, colormap, accent.get_str(),
      &acc_color)) {
    std::cerr << "Couldn't allocate xft color " << accent.get_str() << "\n";
  }

  xft_color_free(&fg_color);
  if (!XftColorAllocName(display, visual_ptr, colormap, fgc.get_str(),
      &fg_color)) {
    std::cerr << "Couldn't allocate xft color " << fgc.get_str() << "\n";
  }
}

void
X::copy_area(xcb_drawable_t src, xcb_drawable_t dst, int16_t src_x,
             int16_t dst_x, uint16_t width, uint16_t height)
{
  // TODO: what's the significance of GC_DRAW here?
  xcb_copy_area(connection, src, dst, gc[GC_DRAW], src_x, 0, dst_x, 0, width,
                height);
}

void
X::create_gc(xcb_pixmap_t pixmap) {
  xcb_create_gc(connection, gc[GC_DRAW],   pixmap, XCB_GC_FOREGROUND, fgc.val());
  xcb_create_gc(connection, gc[GC_ACCENT], pixmap, XCB_GC_FOREGROUND, accent.val());
  xcb_create_gc(connection, gc[GC_CLEAR],  pixmap, XCB_GC_FOREGROUND, bgc.val());
  xcb_create_gc(connection, gc[GC_ATTR],   pixmap, XCB_GC_FOREGROUND, ugc.val());
}

void
X::create_pixmap(xcb_pixmap_t pid, xcb_drawable_t drawable, uint16_t width, uint16_t height)
{
  xcb_create_pixmap(connection, get_depth(), pid, drawable, width, height);
}

void
X::free_pixmap(xcb_pixmap_t pixmap) {
  xcb_free_pixmap(connection, pixmap);
}

void
X::create_window(xcb_window_t wid,
    int16_t x, int16_t y, uint16_t width, uint16_t height, uint16_t _class,
    xcb_visualid_t visual, uint32_t value_mask, const void *value_list)
{
  xcb_create_window(connection, get_depth(), wid, screen->root, x, y, width,
      height, 0, _class, visual, value_mask, value_list);
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
X::fill_rect(xcb_drawable_t d, uint32_t gc_index, int16_t x, int16_t y,
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
X::get_visual_info(int64_t vinfo_mask, XVisualInfo *vinfo_template,
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


std::string
X::get_window_title(Window win) {
  XClassHint hint;
  XGetClassHint(display, win, &hint);
  std::string ret(hint.res_class);
  XFree(hint.res_class);
  XFree(hint.res_name);
  return ret;
}


std::vector<Window>
X::get_windows() {
  Window root = get_default_root_window();
  auto client_list =
       get_property<Window>(root, XA_WINDOW, "_NET_CLIENT_LIST")
    ?: get_property<Window>(root, XA_CARDINAL, "_WIN_CLIENT_LIST");

  if (!client_list) {
    std::cerr << "Cannot get client list properties. "
                 "(_NET_CLIENT_LIST or _WIN_CLIENT_LIST)\n";
    exit(EXIT_FAILURE);
  }

  return client_list.value();
}


uint32_t
X::get_current_workspace() {
  Window root = get_default_root_window();
  auto cur_desktop =
       get_property<uint64_t>(root, XA_CARDINAL, "_NET_CURRENT_DESKTOP")
    ?: get_property<uint64_t>(root, XA_CARDINAL, "_WIN_WORKSPACE");

  if (!cur_desktop) {
    std::cerr << "Cannot get current desktop properties. "
                 "(_NET_CURRENT_DESKTOP or _WIN_WORKSPACE property)\n";
    exit(EXIT_FAILURE);
  }

  return cur_desktop.value().at(0);
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
X::xft_char_width(uint16_t ch) {
  auto load_char = [&] {
    XGlyphInfo gi;
    XftFont *font = fonts.drawable_font(ch).xft_ft;
    FT_UInt glyph = XftCharIndex(display, font, (FcChar32) ch);
    XftFontLoadGlyphs(display, font, FcFalse, &glyph, 1);
    XftGlyphExtents(display, font, &glyph, 1, &gi);
    XftFontUnloadGlyphs(display, font, &glyph, 1);
    return gi.xOff;
  };

  auto itr = [&] {
    std::shared_lock lock{_char_widths_mutex};
    return xft_char_widths.find(ch);
  }();

  if (itr == xft_char_widths.end()) {
    std::unique_lock lock{_char_widths_mutex};
    itr = xft_char_widths.insert({ch, load_char()}).first;
  }
  return itr->second;
}

void
X::xft_color_free(XftColor* color) {
  XftColorFree(display, visual_ptr, colormap, color);
}

Visual *
X::xft_default_visual(int screen) {
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
X::draw_ucs2_string(XftDraw* draw, const std::vector<uint16_t>& str, size_t x) {
  // TODO: currently the proper font for this character needs to be found twice.
  // This can definitely be optimized.
  // Also, group consecutive characters of the same font so they can be printed
  // in bulk.

  for (auto ch : str) {
    auto& font = fonts.drawable_font(ch);
    const int y = BAR_HEIGHT / 2 + font.height / 2
                  - font.descent + font.offset;
    XftDrawString16(draw, &fg_color, font.xft_ft, x, y, &ch, 1);
    x += xft_char_width(ch);
  }
}

void
X::draw_ucs2_string_accent(XftDraw* draw, const std::vector<uint16_t>& str, size_t x) {
  // TODO: currently the proper font for this character needs to be found twice.
  // This can definitely be optimized.
  // Also, group consecutive characters of the same font so they can be printed
  // in bulk.

  for (auto ch : str) {
    auto& font = fonts.drawable_font(ch);
    const int y = BAR_HEIGHT / 2 + font.height / 2
                  - font.descent + font.offset;
    XftDrawString16(draw, &acc_color, font.xft_ft, x, y, &ch, 1);
    x += xft_char_width(ch);
  }
}

void
X::xft_font_close(XftFont *xft) {
  XftFontClose(display, xft);
}

uint8_t
X::get_depth() {
  return (visual == screen->root_visual) ? XCB_COPY_FROM_PARENT : 32;
}


// helpers

xcb_atom_t get_atom(xcb_connection_t *conn, const char *name) {
  std::unique_ptr<xcb_intern_atom_reply_t, decltype(std::free) *> reply {
      xcb_intern_atom_reply(conn,
          xcb_intern_atom(conn, 0, static_cast<uint16_t>(strlen(name)),
                          name),
          nullptr), std::free };
  return reply ? reply->atom : XCB_NONE;
}

xcb_connection_t *get_connection() {
  auto *conn = xcb_connect(nullptr, nullptr);
  if (xcb_connection_has_error(conn)) {
    std::cerr << "Cannot create X connection.\n";
    exit(EXIT_FAILURE);
  }
  return conn;
}
