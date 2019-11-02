#pragma once

#include "modules/module.h"
#include "window.h"
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


/** section_t
 * A bar is broken up into three sections; left, middle, and right. This class
 * stores the modules in a given section, manages their threads and collects
 * their current values into one representation of the state of the section at
 * the time which it was called.
 */
template <typename ...Mod>
struct section_t {
  section_t();
  std::array<std::string, sizeof...(Mod)> collect();

  std::tuple<Module<Mod>...> _modules;
  std::array<std::thread, sizeof...(Mod)> _module_threads;
};

template <typename ...Mod>
section_t<Mod...>::section_t()
  : _module_threads(
      [this]<size_t... I>(std::index_sequence<I...>) -> decltype(_module_threads) {
        return { std::thread(std::ref(std::get<I>(_modules)))... };
      }(std::make_index_sequence<sizeof...(Mod)>{}))
{}

template <typename ...Mod>
std::array<std::string, sizeof...(Mod)>
section_t<Mod...>::collect() {
  // TODO: would this be better with a static vector?
  std::array<std::string, sizeof...(Mod)> module_strings;

  std::apply(
      [&](auto&... t) {
        int i = 0;
        (( module_strings[i++] = t.get() ), ...);
      },
      _modules);

  return module_strings;
}


/** bar_t
 * The bar_t class maintains the three different sections and the window
 * displaying the bar itself. It will also draw each section into the bar.
 */
template <size_t origin_x, size_t origin_y,
          size_t width, size_t height,
          typename Left, typename Middle, typename Right>
struct bar_t {
  bar_t();

  void operator()();
  ucs2_and_width utf8_to_ucs2(const std::string& text);
  void update();

  X& _x;  // TODO: Bars should own its own default constructed X.
  /* std::condition_variable _condvar;  // TODO: Each bar should have it's own synchronization. */
  std::tuple<Left, Middle, Right> _sections;
  BarWindow _win;
};

template <size_t origin_x, size_t origin_y,
          size_t width, size_t height,
          typename Left, typename Middle, typename Right>
bar_t<origin_x, origin_y, width, height, Left, Middle, Right>::bar_t()
  : _x(X::Instance()), _win(_x, origin_x, origin_y, width, height)
{}

template <size_t origin_x, size_t origin_y,
          size_t width, size_t height,
          typename Left, typename Middle, typename Right>
void
bar_t<origin_x, origin_y, width, height, Left, Middle, Right>::operator()()
{
  while (!_x.connection_has_error()) {
    // draw the bar
    _win.clear();
    update();
    _win.render();

    // wait for an event
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    condvar.wait(lock);
  }
}

template <size_t origin_x, size_t origin_y,
          size_t width, size_t height,
          typename Left, typename Middle, typename Right>
void
bar_t<origin_x, origin_y, width, height, Left, Middle, Right>::update()
{
  // add in left modules up to the entire bar
  size_t left = 0;
  for (const auto& str : std::get<0>(_sections).collect()) {
    ucs2_and_width parsed = utf8_to_ucs2(str);
    if (left + parsed.second > width) {
      break;
    }
    // TODO: drawing to the window should be done within the window
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
}

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


// TODO: how to share references to common modules?
// TODO: proper encapsulation with a class
/** Bars
 * Run each bar in their own thread.
 */
template <typename ...Bar>
struct Bars {
  Bars()
    : _threads(
        [this]<size_t... I>(std::index_sequence<I...>) -> decltype(_threads) {
          return { std::thread(std::ref(std::get<I>(_bars)))... };
        }(std::make_index_sequence<sizeof...(Bar)>{}))
  {}
  ~Bars() {
    for (auto& t : _threads)
      t.join();
  }

  std::tuple<Bar...> _bars;
  std::array<std::thread, sizeof...(Bar)> _threads;
};
