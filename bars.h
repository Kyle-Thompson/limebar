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

#include "bar_color.h"
#include "font.h"
#include "modules/module.h"
#include "pixmap.h"
#include "window.h"

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


/**
 * Convert a parameter pack of ModuleContainers to a tuple of modules that can
 * be used to initialize a Section.
 */
template <typename... Mods>
std::tuple<Mods&...>
make_section(ModuleContainer<Mods>&... mods) {
  return std::tuple<Mods&...>{*mods...};
}


/** Section
 * A bar is broken up into three sections; left, middle, and right. This class
 * stores the modules in a given section, manages their threads and collects
 * their current values into one representation of the state of the section at
 * the time which it was called.
 *
 * TODO: how to verify that Mods is a tuple?
 */
template <typename Mods>
class Section {
 public:
  Section(std::condition_variable* cond, BarWindow* win, Mods&& mods)
      : _pixmap(win->generate_mod_pixmap()), _modules(std::move(mods)) {
    std::apply([&](auto&&... mods) { (mods.subscribe(cond), ...); }, _modules);
  }

  ModulePixmap& collect() {
    _pixmap.clear();
    std::apply([this](auto&&... mods) { (mods.get(&_pixmap), ...); }, _modules);
    return _pixmap;
  }

 private:
  ModulePixmap _pixmap;
  Mods _modules;
};


/** Bar
 * The Bar class maintains the three different sections and the window
 * displaying the bar itself. It will also draw each section into the bar.
 */
template <typename Left, typename Middle, typename Right>
class Bar {
 public:
  Bar(dimension_t d, BarColors&& colors, Fonts&& fonts, Left left,
      Middle middle, Right right);

  void operator()();
  void update();

 private:
  std::condition_variable _condvar;
  size_t _origin_x, _origin_y, _width, _height;
  BarWindow _win;
  Section<Left> _left;
  Section<Middle> _middle;
  Section<Right> _right;
};

template <typename Left, typename Middle, typename Right>
Bar<Left, Middle, Right>::Bar(dimension_t d, BarColors&& colors, Fonts&& fonts,
                              Left left, Middle middle, Right right)
    : _origin_x(d.origin_x)
    , _origin_y(d.origin_y)
    , _width(d.width)
    , _height(d.height)
    , _win(std::move(colors), std::move(fonts), _origin_x, _origin_y, _width,
           _height)
    , _left(&_condvar, &_win, std::move(left))
    , _middle(&_condvar, &_win, std::move(middle))
    , _right(&_condvar, &_win, std::move(right)) {
}

template <typename Left, typename Middle, typename Right>
void
Bar<Left, Middle, Right>::operator()() {
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
Bar<Left, Middle, Right>::update() {
  _win.update_left(_left.collect());
  _win.update_right(_right.collect());
  _win.update_middle(_middle.collect());
}


/** Bars
 * Run each bar in their own thread.
 */
template <typename... Bar>
class BarContainer {
 public:
  explicit BarContainer(Bar&... bars)
      : _threads{std::thread(std::ref(bars))...} {}

  ~BarContainer() {
    for (auto& t : _threads) {
      t.join();
    }
  }

  BarContainer(const BarContainer&) = delete;
  BarContainer(BarContainer&&) = delete;
  BarContainer& operator=(const BarContainer&) = delete;
  BarContainer& operator=(BarContainer&&) = delete;

 private:
  // TODO: jthreads
  std::array<std::thread, sizeof...(Bar)> _threads;
};
