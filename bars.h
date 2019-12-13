#pragma once

#include <bits/stdint-uintn.h>  // int_t

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>  // size_t
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>  // pair, make_index_sequence

#include "modules/module.h"
#include "pixmap.h"
#include "window.h"
#include "x.h"

/** dimension_t
 * Utility struct for passing dimensions to bar_t.
 * NOTE: origin_{x,y} refers to the coordinate pair for the top left pixel.
 */
struct dimension_t {
  size_t origin_x;
  size_t origin_y;
  size_t width;
  size_t height;
};

/** section_t
 * A bar is broken up into three sections; left, middle, and right. This class
 * stores the modules in a given section, manages their threads and collects
 * their current values into one representation of the state of the section at
 * the time which it was called.
 *
 * TODO: how to verify that Mods is a tuple?
 */
template <typename Mods>
struct section_t {
  section_t(std::condition_variable& cond, BarWindow& win, Mods&& mods)
      : _pixmap(win.generate_mod_pixmap()), _modules(std::move(mods)) {
    std::apply([&](auto&&... mods) { (mods.subscribe(&cond), ...); }, _modules);
  }

  ModulePixmap& collect() {
    _pixmap.clear();
    std::apply([&](auto&&... mods) { (mods.get(_pixmap), ...); }, _modules);
    return _pixmap;
  }

  ModulePixmap _pixmap;
  Mods _modules;
};

/** bar_t
 * The bar_t class maintains the three different sections and the window
 * displaying the bar itself. It will also draw each section into the bar.
 */
template <typename Left, typename Middle, typename Right>
struct bar_t {
  bar_t(dimension_t d, Left left, Middle middle, Right right);

  void operator()();
  void update();

  std::condition_variable _condvar;
  size_t _origin_x, _origin_y, _width, _height;
  BarWindow _win;
  section_t<Left> _left;
  section_t<Middle> _middle;
  section_t<Right> _right;
};

template <typename Left, typename Middle, typename Right>
bar_t<Left, Middle, Right>::bar_t(dimension_t d, Left left, Middle middle,
                                  Right right)
    : _origin_x(d.origin_x)
    , _origin_y(d.origin_y)
    , _width(d.width)
    , _height(d.height)
    , _win(_origin_x, _origin_y, _width, _height)
    , _left(_condvar, _win, std::move(left))
    , _middle(_condvar, _win, std::move(middle))
    , _right(_condvar, _win, std::move(right)) {
}

template <typename Left, typename Middle, typename Right>
void
bar_t<Left, Middle, Right>::operator()() {
  while (true) {
    _win.clear();
    update();
    _win.render();

    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    _condvar.wait(lock);
  }
}

template <typename Left, typename Middle, typename Right>
void
bar_t<Left, Middle, Right>::update() {
  _win.update_left(_left.collect());
  _win.update_right(_right.collect());
  _win.update_middle(_middle.collect());
}

/** Bars
 * Run each bar in their own thread.
 */
template <typename... Bar>
struct Bars {
  Bars(Bar&... bars) : _threads{std::thread(std::ref(bars))...} {}
  ~Bars() {
    for (auto& t : _threads) t.join();
  }

  // TODO: jthreads
  std::array<std::thread, sizeof...(Bar)> _threads;
};
