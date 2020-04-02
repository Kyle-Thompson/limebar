#include "x.h"

#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xutil.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xcb_xrm.h>

#include <cstdlib>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
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

std::pair<xcb_visualid_t, Visual*>
X11::get_visual() {
  XVisualInfo xv{.depth = 32};
  int results = 0;
  std::unique_ptr<XVisualInfo, decltype(XFree)*> result_ptr{
      XGetVisualInfo(_display, VisualDepthMask, &xv, &results), XFree};

  using ret_t = decltype(get_visual());
  return results > 0 ? ret_t(result_ptr->visualid, result_ptr->visual)
                     : ret_t(_screen->root_visual, DefaultVisual(_display, 0));
}

X11::X11() {
  if (_display = XOpenDisplay(nullptr); _display == nullptr) {
    std::cerr << "Couldnt open display\n";
    exit(EXIT_FAILURE);
  }

  if (_connection = XGetXCBConnection(_display);
      _connection == nullptr || xcb_connection_has_error(_connection) > 0) {
    std::cerr << "Couldn't connect to X\n";
    exit(EXIT_FAILURE);
  }

  XSetEventQueueOwner(_display, XCBOwnsEventQueue);

  _screen = xcb_setup_roots_iterator(xcb_get_setup(_connection)).data;
  std::tie(_visual, _visual_ptr) = get_visual();
  _colormap = xcb_generate_id(_connection);
  xcb_create_colormap(_connection, XCB_COLORMAP_ALLOC_NONE, _colormap,
                      _screen->root, _visual);
  _gc_bg = generate_id();
}

X11::~X11() {
  if (_gc_bg > 1) {
    xcb_free_gc(_connection, _gc_bg);
  }
  if (_connection != nullptr) {
    xcb_disconnect(_connection);
  }
}

X11&
X11::Instance() {
  static X11 instance;
  return instance;
}

void
X11::init() {
  if (!XInitThreads()) {
    std::cerr << "Failed to initialize threading for Xlib\n";
    exit(EXIT_FAILURE);
  }
}

void
X11::activate_window(Window win) {
  // TODO: why do we need a new display here? why not use X11's display member?
  Display* disp = XOpenDisplay(nullptr);

  XEvent event;
  int64_t mask = SubstructureRedirectMask | SubstructureNotifyMask;

  event.xclient = {
      .type = ClientMessage,
      .serial = 0,
      .send_event = True,
      .display = disp,
      .window = win,
      .message_type = XInternAtom(disp, "_NET_ACTIVE_WINDOW", False),
      .format = 32,
      .data = {{
          0,
      }}};

  XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event);
  XMapRaised(disp, win);
  XCloseDisplay(disp);
}

void
X11::switch_desktop(int desktop) {
  // TODO: why do we need a new display here? why not use X11's display member?
  Display* disp = XOpenDisplay(nullptr);
  Window win = DefaultRootWindow(disp);

  XEvent event;
  event.xclient = {
      .type = ClientMessage,
      .serial = 0,
      .send_event = True,
      .display = disp,
      .window = win,
      .message_type = XInternAtom(disp, "_NET_CURRENT_DESKTOP", False),
      .format = 32,
      .data = {{
          0,
      }}};
  event.xclient.data.l[0] = desktop;

  int64_t mask = SubstructureRedirectMask | SubstructureNotifyMask;
  XSendEvent(disp, win, False, mask, &event);
  XCloseDisplay(disp);
}

X11::font_color_t
X11::create_font_color(const rgba_t& rgb) {
  return font_color_t(this, rgb);
}

X11::font_t
X11::create_font(const char* pattern, int offset) {
  return font_t(_display, pattern, offset);
}

// TODO: refactor into multiple functions
X11::window_t
X11::create_window(rectangle_t dim, const rgba_t& rgb, bool reserve_space) {
  const auto [x, y, width, height] = dim;
  window_t win(this, width, height);

  const uint32_t value_mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
                              XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK |
                              XCB_CW_COLORMAP;
  const std::array<uint32_t, 5> mask{
      *rgb.val(), *rgb.val(), FORCE_DOCK,
      XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
          XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE,
      _colormap};
  xcb_create_window(_connection, get_depth(), win._id, _screen->root, x, y,
                    width, height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, _visual,
                    value_mask, mask.data());

  if (!reserve_space) {
    return win;
  }

  std::array<xcb_atom_t, atom_names_size> atom_list;
  std::transform(
      atom_names.begin(), atom_names.end(), atom_list.begin(),
      [this](auto name) {
        auto* reply =
            xcb_intern_atom_reply(_connection, get_atom_by_name(name), nullptr);
        if (!reply) {
          std::cerr << "error: atom reply failed.\n";
          exit(EXIT_FAILURE);
        }
        return (std::unique_ptr<xcb_intern_atom_reply_t, decltype(std::free)*>{
                    reply, std::free})
            ->atom;
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

  xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, win._id,
                      atom_list[NET_WM_WINDOW_TYPE], XCB_ATOM_ATOM, 32, 1,
                      &atom_list[NET_WM_WINDOW_TYPE_DOCK]);
  xcb_change_property(_connection, XCB_PROP_MODE_APPEND, win._id,
                      atom_list[NET_WM_STATE], XCB_ATOM_ATOM, 32, 2,
                      &atom_list[NET_WM_STATE_STICKY]);
  xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, win._id,
                      atom_list[NET_WM_DESKTOP], XCB_ATOM_CARDINAL, 32, 1,
                      (std::array<uint32_t, 1>{0u - 1u}).data());
  xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, win._id,
                      atom_list[NET_WM_STRUT_PARTIAL], XCB_ATOM_CARDINAL, 32,
                      12, strut.data());
  xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, win._id,
                      atom_list[NET_WM_STRUT], XCB_ATOM_CARDINAL, 32, 4,
                      strut.data());
  xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, win._id,
                      XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 3, "bar");
  xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, win._id,
                      XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, 12,
                      "lemonbar\0Bar");

  win.make_visible();

  // Make sure that the window really gets in the place it's supposed to be
  // Some WM such as Openbox need this
  const std::array<uint32_t, 2> xy{static_cast<uint32_t>(x),
                                   static_cast<uint32_t>(y)};
  win.configure(XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, xy.data());

  // Set the WM_NAME atom to the user specified value
  if constexpr (WM_NAME != nullptr) {
    xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, win._id,
                        XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(WM_NAME),
                        WM_NAME);
  }

  // set the WM_CLASS atom instance to the executable name
  if (!WM_CLASS.empty()) {
    constexpr int size = WM_CLASS.size() + 6;
    std::array<char, size> wm_class = {0};

    // WM_CLASS is nullbyte seperated: WM_CLASS + "\0Bar\0"
    strncpy(wm_class.data(), WM_CLASS.data(), WM_CLASS.size());
    strcpy(wm_class.data() + WM_CLASS.size(), "\0Bar");

    xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, win._id,
                        XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, size,
                        wm_class.data());
  }

  return win;
}

X11::rdb_t
X11::create_resource_database() {
  return rdb_t(this);
}


std::unique_ptr<xcb_generic_event_t, decltype(std::free)*>
X11::wait_for_event() {
  return {xcb_wait_for_event(_connection), std::free};
}

Window
X11::get_default_root_window() {
  return DefaultRootWindow(_display);
}

Atom
X11::get_intern_atom() {
  return XInternAtom(_display, "UTF8_STRING", 0);
}

xcb_intern_atom_cookie_t
X11::get_atom_by_name(const char* name) {
  return xcb_intern_atom(_connection, 0, strlen(name), name);
}


std::string
X11::get_window_title(Window win) {
  XClassHint hint;
  XGetClassHint(_display, win, &hint);
  std::string ret(hint.res_class);
  if (hint.res_class != nullptr) {
    XFree(hint.res_class);
  }
  if (hint.res_name != nullptr) {
    XFree(hint.res_name);
  }
  return ret;
}


std::vector<Window>
X11::get_windows() {
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


Window
X11::get_active_window() {
  Window root = get_default_root_window();
  const char* query = "_NET_ACTIVE_WINDOW";
  return get_property<Window>(root, XA_WINDOW, query).value().at(0);
}


std::vector<std::string>
X11::get_workspace_names() {
  Window root = get_default_root_window();
  auto atom = get_intern_atom();
  // TODO: holy shit this is hacky. Fix this.
  // likely solution: create specification on string to avoid this
  auto vec = get_property<char>(root, atom, "_NET_DESKTOP_NAMES").value();
  char* chars = vec.data();
  std::vector<std::string> names;
  for (char* str = chars; str - chars < vec.size(); str += strlen(str) + 1) {
    names.emplace_back(str);
  }
  return names;
}


uint32_t
X11::get_current_workspace() {
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


std::optional<uint32_t>
X11::get_workspace_of_window(Window window) {
  auto p = get_property<uint32_t>(window, XA_CARDINAL, "_NET_WM_DESKTOP");
  if (!p.has_value()) {
    std::cerr << "No desktop found for window " << get_window_title(window)
              << " (" << window << ")\n";
    return std::nullopt;
  }
  return p.value()[0];
}


X11::font_color_t::font_color_t(X11* x, const rgba_t& rgb)
    : _x(x)
    , _color([this, rgb] {
      XftColor color;
      // TODO: use XftColorAllocValue instead
      if (XftColorAllocName(_x->_display, _x->_visual_ptr, _x->_colormap,
                            rgb.get_str(), &color) == 0) {
        std::cerr << "Couldn't allocate xft color " << rgb.get_str() << "\n";
      }
      return color;
    }()) {
}

X11::font_color_t::~font_color_t() {
  XftColorFree(_x->_display, _x->_visual_ptr, _x->_colormap, &_color);
}


X11::font_t::font_t(Display* dpy, const char* pattern, int offset)
    : _display(dpy)
    , _xft_ft(XftFontOpenName(_display, 0, pattern)) {
  if (_xft_ft == nullptr) {
    std::cerr << "Could not load font " << pattern << "\n";
    exit(EXIT_FAILURE);
  }

  _descent = _xft_ft->descent;
  _height = _xft_ft->ascent + _descent;
  this->_offset = offset;
}

X11::font_t::~font_t() {
  for (auto& [ch, glyph] : _glyph_map) {
    XftFontUnloadGlyphs(_display, _xft_ft, &glyph.id, 1);
  }
  XftFontClose(_display, _xft_ft);
}

void
X11::font_t::draw_ucs2(XftDraw* draw, font_color_t* color, const ucs2& str,
                       uint16_t height, size_t x) {
  const int y = static_cast<int>(height) / 2 + _height / 2 - _descent + _offset;
  XftDrawString16(draw, color->get(), _xft_ft, x, y, str.data(), str.size());
}

auto
X11::font_t::get_glyph(uint16_t ch) -> glyph_map_itr {
  auto itr = [this, ch] {
    std::shared_lock lock{_mu};
    return _glyph_map.find(ch);
  }();

  if (itr == _glyph_map.end() &&
      XftCharExists(_display, _xft_ft, static_cast<FcChar32>(ch)) == True) {
    std::unique_lock lock{_mu};
    return _glyph_map.emplace(std::make_pair(ch, create_glyph(ch))).first;
  }
  return itr;
}

bool
X11::font_t::has_glyph(uint16_t ch) {
  return get_glyph(ch) != _glyph_map.end();
}

uint16_t
X11::font_t::char_width(uint16_t ch) {
  return get_glyph(ch)->second.info.xOff;
}

size_t
X11::font_t::string_size(const ucs2& str) {
  return std::accumulate(
      str.begin(), str.end(), 0,
      [this](size_t size, uint16_t ch) { return size + char_width(ch); });
}

auto
X11::font_t::create_glyph(uint16_t ch) -> glyph_t {
  XGlyphInfo glyph_info;
  FT_UInt glyph_id = XftCharIndex(_display, _xft_ft, static_cast<FcChar32>(ch));
  XftFontLoadGlyphs(_display, _xft_ft, FcFalse, &glyph_id, 1);
  XftGlyphExtents(_display, _xft_ft, &glyph_id, 1, &glyph_info);
  return {glyph_id, glyph_info};
}


X11::window_t::window_t(X11* x, uint16_t width, uint16_t height)
    : _x(x), _id(x->generate_id()), _width(width), _height(height) {
}

X11::window_t::window_t(window_t&& rhs) noexcept
    : _x(std::exchange(rhs._x, nullptr))
    , _id(rhs._id)
    , _width(rhs._width)
    , _height(rhs._height) {
}

X11::window_t::~window_t() {
  if (_x != nullptr) {
    xcb_destroy_window(_x->_connection, _id);
  }
}

void
X11::window_t::make_visible() {
  xcb_map_window(_x->_connection, _id);
}

void
X11::window_t::configure(uint16_t mask, const void* list) {
  xcb_configure_window(_x->_connection, _id, mask, list);
}

void
X11::window_t::copy_from(const pixmap_t& rhs, coordinate_t src,
                         coordinate_t dst, uint16_t width, uint16_t height) {
  xcb_copy_area(_x->_connection, rhs._id, _id, _x->_gc_bg, src.x, 0, dst.x, 0,
                width, height);
}

X11::pixmap_t
X11::window_t::create_pixmap() const {
  return pixmap_t(_x, _id, _width, _height);
}

void
X11::window_t::create_gc(const rgba_t& rgb) const {
  xcb_create_gc(_x->_connection, _x->_gc_bg, _id, XCB_GC_FOREGROUND, rgb.val());
}


X11::pixmap_t::pixmap_t(X11* x, xcb_drawable_t drawable, uint16_t width,
                        uint16_t height)
    : _x(x), _id(x->generate_id()), _width(width), _height(height) {
  xcb_create_pixmap(_x->_connection, _x->get_depth(), _id, drawable, width,
                    height);
}

X11::pixmap_t::pixmap_t(pixmap_t&& rhs) noexcept
    : _x(std::exchange(rhs._x, nullptr))
    , _id(rhs._id)
    , _width(rhs._width)
    , _height(rhs._height) {
}


X11::pixmap_t::~pixmap_t() {
  if (_x != nullptr) {
    xcb_free_pixmap(_x->_connection, _id);
  }
}

void
X11::pixmap_t::clear() {
  xcb_rectangle_t rect = {0, 0, _width, _height};
  xcb_poly_fill_rectangle(_x->_connection, _id, _x->_gc_bg, 1, &rect);
}

void
X11::pixmap_t::copy_from(const pixmap_t& rhs, coordinate_t src,
                         coordinate_t dst, uint16_t width, uint16_t height) {
  xcb_copy_area(_x->_connection, rhs._id, _id, _x->_gc_bg, src.x, 0, dst.x, 0,
                width, height);
}

XftDraw*
X11::pixmap_t::create_xft_draw() const {
  return XftDrawCreate(_x->_display, _id, _x->_visual_ptr, _x->_colormap);
}


X11::rdb_t::rdb_t(X11* x)
    : _db(xcb_xrm_database_from_default(x->_connection)) {}

X11::rdb_t::~rdb_t() {
  if (_db != nullptr) {
    xcb_xrm_database_free(_db);
  }
}

X11::rdb_t::rdb_t(rdb_t&& rhs) noexcept
    : _db(std::exchange(rhs._db, nullptr)) {}

template<>
std::string
X11::rdb_t::get<std::string>(const char* query) {
  char* str = nullptr;
  xcb_xrm_resource_get_string(_db, query, nullptr, &str);
  std::string ret(str);
  free(str);
  return ret;
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
