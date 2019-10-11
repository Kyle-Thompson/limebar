#pragma once

#include "modules/module.h"
#include "x.h"

#include <algorithm>
#include <array>
#include <bits/stdint-uintn.h>  // int_t
#include <chrono>
#include <cstddef>  // size_t
#include <mutex>
#include <sstream>
#include <thread>
#include <tuple>
#include <utility>  // pair, make_index_sequence
#include <vector>

using ucs2 = std::vector<uint16_t>;
using ucs2_and_width = std::pair<ucs2, size_t>;

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
std::array<xcb_atom_t, size> atom_list;

template <typename ...Mod>
struct section_t {
  using strs_and_sizes = std::pair<std::array<ucs2,   sizeof...(Mod)>,
                                   std::array<size_t, sizeof...(Mod)>>;

  section_t()
    : _module_threads(
        [this]<size_t... I>(std::index_sequence<I...>) -> decltype(_module_threads) {
          return { std::thread(std::ref(std::get<I>(_modules)))... };
        }(std::make_index_sequence<sizeof...(Mod)>{})
      ) {}

  auto collect() {
    // TODO: construct in place
    std::array<std::string, sizeof...(Mod)> module_strings;

    std::apply(
        [&](auto&... t) {
          int i = 0;
          (( module_strings[i++] = t.get() ), ...);
        },
        _modules);

    return module_strings;
  }

  std::tuple<Module<Mod>...> _modules;
  std::array<std::thread, sizeof...(Mod)> _module_threads;
};


template <size_t origin_x, size_t origin_y,
          size_t width, size_t height,
          typename Left, typename Middle, typename Right>
struct bar_t {
  bar_t() : _x(X::Instance()), _window(_x.generate_id()), _pixmap(_x.generate_id()) {
    // TODO: investigate if this should be redone per this leftover comment:
    // As suggested fetch all the cookies first (yum!) and then retrieve the
    // atoms to exploit the async'ness
    std::transform(atom_names.begin(), atom_names.end(), atom_list.begin(),
        [this](auto name){
          xcb_intern_atom_reply_t *atom_reply =
              _x.get_intern_atom_reply(name);
          if (!atom_reply) {
            fprintf(stderr, "atom reply failed.\n");
            exit(EXIT_FAILURE);
          }
          auto ret = atom_reply->atom;
          free(atom_reply);  // TODO: use unique_ptr
          return ret;
        });

    // create a window with width and height
    const uint32_t mask[] { *_x.bgc.val(), *_x.bgc.val(), FORCE_DOCK,
      XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
          XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE,
      _x.get_colormap() };

    _x.create_window(_window, origin_x, origin_y, width,
        XCB_WINDOW_CLASS_INPUT_OUTPUT, _x.get_visual(),
        XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT |
            XCB_CW_EVENT_MASK | XCB_CW_COLORMAP,
        mask);

    _x.create_pixmap(_pixmap, _window, width);

    int strut[12] = {0};
    if (origin_y == 0) {  // TODO: Find a better way of determining if this is a top-bar
      strut[2] = BAR_HEIGHT;
      strut[8] = origin_x;
      strut[9] = origin_x + width;
    } else {
      strut[3]  = BAR_HEIGHT;
      strut[10] = origin_x;
      strut[11] = origin_x + width;
    }

    _x.change_property(XCB_PROP_MODE_REPLACE, _window, atom_list[NET_WM_WINDOW_TYPE],   XCB_ATOM_ATOM,     32, 1,  &atom_list[NET_WM_WINDOW_TYPE_DOCK]);
    _x.change_property(XCB_PROP_MODE_APPEND,  _window, atom_list[NET_WM_STATE],         XCB_ATOM_ATOM,     32, 2,  &atom_list[NET_WM_STATE_STICKY]);
    _x.change_property(XCB_PROP_MODE_REPLACE, _window, atom_list[NET_WM_DESKTOP],       XCB_ATOM_CARDINAL, 32, 1,  (const uint32_t []) { 0u - 1u } );
    _x.change_property(XCB_PROP_MODE_REPLACE, _window, atom_list[NET_WM_STRUT_PARTIAL], XCB_ATOM_CARDINAL, 32, 12, strut);
    _x.change_property(XCB_PROP_MODE_REPLACE, _window, atom_list[NET_WM_STRUT],         XCB_ATOM_CARDINAL, 32, 4,  strut);
    _x.change_property(XCB_PROP_MODE_REPLACE, _window, XCB_ATOM_WM_NAME,                XCB_ATOM_STRING,   8,  3,  "bar");
    _x.change_property(XCB_PROP_MODE_REPLACE, _window, XCB_ATOM_WM_CLASS,               XCB_ATOM_STRING,   8,  12, "lemonbar\0Bar");

    _x.map_window(_window);
    _x.create_gc(_pixmap);
    _x.fill_rect(_pixmap, GC_CLEAR, 0, 0, width, height);

    // Make sure that the window really gets in the place it's supposed to be
    // Some WM such as Openbox need this
    const uint32_t xy[] {
        static_cast<uint32_t>(origin_x), static_cast<uint32_t>(origin_y) };
    _x.configure_window(_window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, xy);

    // Set the WM_NAME atom to the user specified value
    if constexpr (WM_NAME != nullptr)
      _x.change_property(XCB_PROP_MODE_REPLACE, _window,
          XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(WM_NAME), WM_NAME);

    // set the WM_CLASS atom instance to the executable name
    if (WM_CLASS.size()) {
      constexpr int size = WM_CLASS.size() + 6;
      char wm_class[size] = {0};

      // WM_CLASS is nullbyte seperated: WM_CLASS + "\0Bar\0"
      strncpy(wm_class, WM_CLASS.data(), WM_CLASS.size());
      strcpy(wm_class + WM_CLASS.size(), "\0Bar");

      _x.change_property(XCB_PROP_MODE_REPLACE, _window,
          XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, size, wm_class);
    }

    if (!(_x.xft_draw = (_x.xft_draw_create(_pixmap)))) {
      fprintf(stderr, "Couldn't create xft drawable\n");
    }
    _x.update_gc();
  }

  ~bar_t() {
    _x.destroy_window(_window);
    _x.free_pixmap(_pixmap);
  }

  void operator()() {
    while (!_x.connection_has_error()) {
      _x.fill_rect(_pixmap, GC_CLEAR, 0, 0, width, height);

      // TODO: move this to update()
      // add in left modules up to the entire bar
      size_t left = 0;
      for (const auto& str : std::get<0>(_sections).collect()) {
        ucs2_and_width parsed = utf8_to_ucs2(str);
        if (left + parsed.second > width) {
          break;
        }
        _x.draw_ucs2_string(parsed.first, left);
        left += parsed.second;
      }

      // add right modules up to width - left
      size_t right = 0;
      for (const auto& str : std::get<2>(_sections).collect()) {  // TODO: reverse iterate
        ucs2_and_width parsed = utf8_to_ucs2(str);
        if (right + parsed.second > width - left) {
          break;
        }
        _x.draw_ucs2_string(parsed.first, width - right - parsed.second);
        right += parsed.second;
      }

      // add center modules with whatever space remains in
      // width / 2 - max(left, right)
      int avail_center_space = width / 2 - std::max(left, right);
      int center = 0;

      std::vector<ucs2_and_width> parsed; // TODO: make into array
      for (const auto& str : std::get<1>(_sections).collect()) {
        ucs2_and_width p = utf8_to_ucs2(str);
        if (center + p.second > avail_center_space) {
          break;
        }
        parsed.push_back(std::move(p));
        center += p.second;
      }

      int curr = width / 2 - center / 2;
      for (const auto& p : parsed) {
        _x.draw_ucs2_string(p.first, curr);
        curr += p.second;
      }

      // draw to the bar
      _x.copy_area(_pixmap, _window, 0, 0, width);
      _x.flush();

      {
        std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        condvar.wait(lock);
      }
    }
  }

  ucs2_and_width utf8_to_ucs2(const std::string& text);
  void update() { }

  // module events
  // bar events

  X& _x;  // TODO: Bars should own its own default constructed X.
  /* std::condition_variable _condvar;  // TODO: Each bar should have it's own synchronization. */
  std::tuple<Left, Middle, Right> _sections;
  xcb_window_t _window;
  xcb_pixmap_t _pixmap;
};


// TODO: how to share references to common modules?
// TODO: proper encapsulation with a class
template <typename ...Bar>
struct Bars {
  Bars() : bar_thread(std::ref(std::get<0>(_bars))) {
  }
  ~Bars() { bar_thread.join(); }

  std::tuple<Bar...> _bars;
  /* std::array<std::thread, sizeof...(Bar)> threads; */
  std::thread bar_thread;  // TODO: replace with array for multiple bars
};


template <size_t origin_x, size_t origin_y,
          size_t width, size_t height,
          typename Left, typename Middle, typename Right>
ucs2_and_width
bar_t<origin_x, origin_y, width, height, Left, Middle, Right>::utf8_to_ucs2(
    const std::string& text)
{
  size_t total_width = 0;
  ucs2 str;

  for (uint8_t *utf = (uint8_t *)text.c_str(); utf != (uint8_t *) &*text.end();) {
    uint16_t ucs = 0;
    // ASCII
    if (utf[0] < 0x80) {
      ucs = utf[0];
      utf  += 1;
    }
    // Two byte utf8 sequence
    else if ((utf[0] & 0xe0) == 0xc0) {
      ucs = (utf[0] & 0x1f) << 6 | (utf[1] & 0x3f);
      utf += 2;
    }
    // Three byte utf8 sequence
    else if ((utf[0] & 0xf0) == 0xe0) {
      ucs = (utf[0] & 0xf) << 12 | (utf[1] & 0x3f) << 6 | (utf[2] & 0x3f);
      utf += 3;
    }
    // Four byte utf8 sequence
    else if ((utf[0] & 0xf8) == 0xf0) {
      ucs = 0xfffd;
      utf += 4;
    }
    // Five byte utf8 sequence
    else if ((utf[0] & 0xfc) == 0xf8) {
      ucs = 0xfffd;
      utf += 5;
    }
    // Six byte utf8 sequence
    else if ((utf[0] & 0xfe) == 0xfc) {
      ucs = 0xfffd;
      utf += 6;
    }
    // Not a valid utf-8 sequence
    else {
      ucs = utf[0];
      utf += 1;
    }

    str.push_back(ucs);
    total_width += _x.xft_char_width(ucs);
  }

  return std::make_pair(str, total_width);
}
