#include "x.h"

#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xutil.h>
#include <bits/stdint-intn.h>
#include <bits/stdint-uintn.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xcb_xrm.h>

#include <cstdlib>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "color.h"
#include "config.h"

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

static constexpr size_t atom_names_size = 8;
static constexpr std::array<const char*, atom_names_size> atom_names{
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

xcb_visualid_t
X::get_visual() {
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
  visual_ptr = DefaultVisual(display, 0);
  return screen->root_visual;
}

X::X() {
  if (!(display = XOpenDisplay(nullptr))) {
    std::cerr << "Couldnt open display\n";
    exit(EXIT_FAILURE);
  }

  if (!(connection = XGetXCBConnection(display)) ||
      xcb_connection_has_error(connection)) {
    std::cerr << "Couldn't connect to X\n";
    exit(EXIT_FAILURE);
  }

  XSetEventQueueOwner(display, XCBOwnsEventQueue);

  if (!(database = xcb_xrm_database_from_default(connection))) {
    std::cerr << "Could not connect to database\n";
    exit(EXIT_FAILURE);
  }

  screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
  /* Try to get a RGBA visual and build the colormap for that */
  visual = get_visual();
  colormap = xcb_generate_id(connection);
  xcb_create_colormap(connection, XCB_COLORMAP_ALLOC_NONE, colormap,
                      screen->root, visual);

  gc_bg = generate_id();
}

X::~X() {
  if (gc_bg) xcb_free_gc(connection, gc_bg);

  if (connection) xcb_disconnect(connection);
  if (database) xcb_xrm_database_free(database);
}

X&
X::Instance() {
  static X instance;
  return instance;
}


std::string
X::get_string_resource(const char* query) {
  char* str = nullptr;
  xcb_xrm_resource_get_string(database, query, nullptr, &str);
  std::string ret(str);
  free(str);
  return ret;
}

void
X::activate_window(Window win) {
  // TODO: why do we need a new display here? why not use X's display member?
  Display* disp = XOpenDisplay(nullptr);

  XEvent event;
  int64_t mask = SubstructureRedirectMask | SubstructureNotifyMask;

  event.xclient.type = ClientMessage;
  event.xclient.serial = 0;
  event.xclient.send_event = True;
  event.xclient.message_type = XInternAtom(disp, "_NET_ACTIVE_WINDOW", False);
  event.xclient.window = win;
  event.xclient.format = 32;
  event.xclient.data.l[0] = 0;
  event.xclient.data.l[1] = 0;
  event.xclient.data.l[2] = 0;
  event.xclient.data.l[3] = 0;
  event.xclient.data.l[4] = 0;

  XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event);
  XMapRaised(disp, win);
  XCloseDisplay(disp);
}

void
X::switch_desktop(int desktop) {
  // TODO: why do we need a new display here? why not use X's display member?
  Display* disp = XOpenDisplay(nullptr);
  Window win = DefaultRootWindow(disp);

  XEvent event;
  int64_t mask = SubstructureRedirectMask | SubstructureNotifyMask;

  event.xclient.type = ClientMessage;
  event.xclient.serial = 0;
  event.xclient.send_event = True;
  event.xclient.message_type = XInternAtom(disp, "_NET_CURRENT_DESKTOP", False);
  event.xclient.window = win;
  event.xclient.format = 32;
  event.xclient.data.l[0] = desktop;
  event.xclient.data.l[1] = 0;
  event.xclient.data.l[2] = 0;
  event.xclient.data.l[3] = 0;
  event.xclient.data.l[4] = 0;

  XSendEvent(disp, win, False, mask, &event);
  XCloseDisplay(disp);
}

void
X::copy_area(xcb_drawable_t src, xcb_drawable_t dst, int16_t src_x,
             int16_t dst_x, uint16_t width, uint16_t height) {
  xcb_copy_area(connection, src, dst, gc_bg, src_x, 0, dst_x, 0, width, height);
}

void
X::create_gc(xcb_pixmap_t pixmap, const rgba_t& rgb) {
  xcb_create_gc(connection, gc_bg, pixmap, XCB_GC_FOREGROUND, rgb.val());
}

void
X::create_pixmap(xcb_pixmap_t pid, xcb_drawable_t drawable, uint16_t width,
                 uint16_t height) {
  xcb_create_pixmap(connection, get_depth(), pid, drawable, width, height);
}

void
X::free_pixmap(xcb_pixmap_t pixmap) {
  xcb_free_pixmap(connection, pixmap);
}

// TODO: refactor into multiple functions
void
X::create_window(xcb_window_t wid, const rgba_t& rgb, int16_t x, int16_t y,
                 uint16_t width, uint16_t height, uint16_t _class,
                 xcb_visualid_t visual, uint32_t value_mask,
                 bool reserve_space) {
  const std::array<uint32_t, 5> mask{
      *rgb.val(), *rgb.val(), FORCE_DOCK,
      XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
          XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE,
      colormap};
  xcb_create_window(connection, get_depth(), wid, screen->root, x, y, width,
                    height, 0, _class, visual, value_mask, mask.data());

  if (!reserve_space) return;

  // TODO: investigate if this should be redone per this leftover comment:
  // As suggested fetch all the cookies first (yum!) and then retrieve the
  // atoms to exploit the async'ness
  std::array<xcb_atom_t, atom_names_size> atom_list;
  std::transform(
      atom_names.begin(), atom_names.end(), atom_list.begin(),
      [this](auto name) {
        std::unique_ptr<xcb_intern_atom_reply_t, decltype(std::free)*>
            atom_reply{get_intern_atom_reply(name), std::free};
        if (!atom_reply) {
          std::cerr << "atom reply failed.\n";
          exit(EXIT_FAILURE);
        }
        return atom_reply->atom;
      });
  std::array<int, 12> strut = {0};
  // TODO: Find a better way of determining if this is a top-bar
  if (y == 0) {
    strut[2] = height;
    strut[8] = x;
    strut[9] = x + width;
  } else {
    strut[3] = height;
    strut[10] = x;
    strut[11] = x + width;
  }

  xcb_change_property(connection, XCB_PROP_MODE_REPLACE, wid,
                      atom_list[NET_WM_WINDOW_TYPE], XCB_ATOM_ATOM, 32, 1,
                      &atom_list[NET_WM_WINDOW_TYPE_DOCK]);
  xcb_change_property(connection, XCB_PROP_MODE_APPEND, wid,
                      atom_list[NET_WM_STATE], XCB_ATOM_ATOM, 32, 2,
                      &atom_list[NET_WM_STATE_STICKY]);
  xcb_change_property(connection, XCB_PROP_MODE_REPLACE, wid,
                      atom_list[NET_WM_DESKTOP], XCB_ATOM_CARDINAL, 32, 1,
                      (std::array<uint32_t, 1>{0u - 1u}).data());
  xcb_change_property(connection, XCB_PROP_MODE_REPLACE, wid,
                      atom_list[NET_WM_STRUT_PARTIAL], XCB_ATOM_CARDINAL, 32,
                      12, strut.data());
  xcb_change_property(connection, XCB_PROP_MODE_REPLACE, wid,
                      atom_list[NET_WM_STRUT], XCB_ATOM_CARDINAL, 32, 4,
                      strut.data());
  xcb_change_property(connection, XCB_PROP_MODE_REPLACE, wid, XCB_ATOM_WM_NAME,
                      XCB_ATOM_STRING, 8, 3, "bar");
  xcb_change_property(connection, XCB_PROP_MODE_REPLACE, wid, XCB_ATOM_WM_CLASS,
                      XCB_ATOM_STRING, 8, 12, "lemonbar\0Bar");

  map_window(wid);

  // Make sure that the window really gets in the place it's supposed to be
  // Some WM such as Openbox need this
  const std::array<uint32_t, 2> xy{static_cast<uint32_t>(x),
                                   static_cast<uint32_t>(y)};
  configure_window(wid, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, xy.data());

  // Set the WM_NAME atom to the user specified value
  if constexpr (WM_NAME != nullptr) {
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, wid,
                        XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(WM_NAME),
                        WM_NAME);
  }

  // set the WM_CLASS atom instance to the executable name
  if (WM_CLASS.size()) {
    constexpr int size = WM_CLASS.size() + 6;
    std::array<char, size> wm_class = {0};

    // WM_CLASS is nullbyte seperated: WM_CLASS + "\0Bar\0"
    strncpy(wm_class.data(), WM_CLASS.data(), WM_CLASS.size());
    strcpy(wm_class.data() + WM_CLASS.size(), "\0Bar");

    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, wid,
                        XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, size,
                        wm_class.data());
  }
}

void
X::destroy_window(xcb_window_t window) {
  xcb_destroy_window(connection, window);
}

void
X::configure_window(xcb_window_t window, uint16_t mask, const void* list) {
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
X::clear_rect(xcb_drawable_t d, uint16_t width, uint16_t height) {
  xcb_rectangle_t rect = {0, 0, width, height};
  xcb_poly_fill_rectangle(connection, d, gc_bg, 1, &rect);
}

Window
X::get_default_root_window() {
  return DefaultRootWindow(display);
}

Atom
X::get_intern_atom() {
  return XInternAtom(display, "UTF8_STRING", 0);
}

XVisualInfo*
X::get_visual_info(int64_t vinfo_mask, XVisualInfo* vinfo_template,
                   int* nitems_return) {
  return XGetVisualInfo(display, vinfo_mask, vinfo_template, nitems_return);
}

xcb_intern_atom_cookie_t
X::get_atom_by_name(const char* name) {
  return xcb_intern_atom(connection, 0, strlen(name), name);
}

xcb_intern_atom_reply_t*
X::get_intern_atom_reply(const char* name) {
  return xcb_intern_atom_reply(connection, get_atom_by_name(name), nullptr);
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

  return *client_list;
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

XftColor
X::alloc_char_color(const rgba_t& rgb) {
  XftColor color;
  if (!XftColorAllocName(display, visual_ptr, colormap, rgb.get_str(),
                         &color)) {
    std::cerr << "Couldn't allocate xft color " << rgb.get_str() << "\n";
  }
  return color;
}

void
X::xft_color_free(XftColor* color) {
  XftColorFree(display, visual_ptr, colormap, color);
}

XftDraw*
X::xft_draw_create(Drawable drawable) {
  return XftDrawCreate(display, drawable, visual_ptr, colormap);
}

void
X::draw_ucs2_string(XftDraw* draw, font_t* font, font_color* color,
                    const std::vector<uint16_t>& str, uint16_t height,
                    size_t x) {
  const int y = static_cast<int>(height) / 2 + font->height / 2 -
                font->descent + font->offset;
  XftDrawString16(draw, color->get(), font->xft_ft, x, y, str.data(),
                  str.size());
}


// helpers
xcb_atom_t
get_atom(xcb_connection_t* conn, const char* name) {
  std::unique_ptr<xcb_intern_atom_reply_t, decltype(std::free)*> reply{
      xcb_intern_atom_reply(
          conn,
          xcb_intern_atom(conn, 0, static_cast<uint16_t>(strlen(name)), name),
          nullptr),
      std::free};
  return reply ? reply->atom : XCB_NONE;
}

xcb_connection_t*
get_connection() {
  auto* conn = xcb_connect(nullptr, nullptr);
  if (int ret = xcb_connection_has_error(conn); ret > 0) {
    std::cerr << "ERROR: Cannot create X connection: ";
    switch (ret) {
      case XCB_CONN_ERROR:
        std::cerr << "XCB_CONN_ERROR, because of socket errors, pipe errors or "
                     "other stream errors.\n";
        break;
      case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
        std::cerr << "XCB_CONN_CLOSED_EXT_NOTSUPPORTED, when extension not "
                     "supported.\n";
        break;
      case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
        std::cerr << "XCB_CONN_CLOSED_MEM_INSUFFICIENT, when memory not "
                     "available.\n";
        break;
      case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
        std::cerr << "XCB_CONN_CLOSED_REQ_LEN_EXCEED, exceeding request length "
                     "that server accepts.\n";
        break;
      case XCB_CONN_CLOSED_PARSE_ERR:
        std::cerr << "XCB_CONN_CLOSED_PARSE_ERR, error during parsing display "
                     "string.\n";
        break;
      case XCB_CONN_CLOSED_INVALID_SCREEN:
        std::cerr << "XCB_CONN_CLOSED_INVALID_SCREEN, because the server does "
                     "not have a screen matching the display.\n";
        break;
      default:
        std::cerr << "UNKNOWN ERROR\n";
    }
    exit(EXIT_FAILURE);
  }
  return conn;
}
