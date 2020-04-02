#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>  // size_t
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>  // pair, make_index_sequence

#include "bar_color.h"
#include "font.h"
#include "modules/module.h"
#include "pixmap.h"
#include "queue.h"
#include "window.h"


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
  Section(StaticWorkQueue<size_t>* queue, BarWindow* win, Mods&& mods)
      : _pixmap(win->generate_mod_pixmap()), _modules(std::move(mods)) {
    std::apply(
        [&](auto&&... mods) {
          (mods.subscribe(register_queue<size_t>(queue, 0)), ...);
        },
        _modules);
  }

  const ModulePixmap& collect() {
    _pixmap.clear();
    std::apply([this](auto&&... mods) { (mods.get(&_pixmap), ...); }, _modules);
    return _pixmap;
  }

  ModulePixmap* get_pixmap() { return &_pixmap; }

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
  Bar(rectangle_t&& d, BarColors&& colors, Fonts&& fonts, Left left,
      Middle middle, Right right);

  void operator()();

 private:
  StaticWorkQueue<size_t> _work;
  size_t _width, _height;
  DS& _ds;
  BarWindow _win;
  Section<Left> _left;
  Section<Middle> _middle;
  Section<Right> _right;
  std::thread _bar_event_thread;
  std::array<std::tuple<size_t, size_t, ModulePixmap*>, 3> _regions;
};

template <typename Left, typename Middle, typename Right>
Bar<Left, Middle, Right>::Bar(rectangle_t&& d, BarColors&& colors,
                              Fonts&& fonts, Left left, Middle middle,
                              Right right)
    : _width(d.width)
    , _height(d.height)
    , _ds(DS::Instance())
    , _win(std::move(colors), std::move(fonts), d)
    , _left(&_work, &_win, std::move(left))
    , _middle(&_work, &_win, std::move(middle))
    , _right(&_work, &_win, std::move(right))
    , _bar_event_thread([this] {
      while (true) {
        auto ev = _ds.wait_for_event();
        // TODO: hover action with XCB_MOTION_NOTIFY
        if (ev && (ev->response_type & 0x7F) == XCB_BUTTON_PRESS) {
          auto* press = reinterpret_cast<xcb_button_press_event_t*>(ev.get());
          for (auto region : _regions) {
            if (press->event_x >= std::get<0>(region) &&
                press->event_x <= std::get<1>(region)) {
              std::get<2>(region)->click(press->event_x - std::get<0>(region),
                                         press->detail);
              break;
            }
          }
        }
      }
    }) {
}

template <typename Left, typename Middle, typename Right>
void
Bar<Left, Middle, Right>::operator()() {
  while (true) {
    _win.reset();

    std::pair<size_t, size_t> p;
    p = _win.update_left(_left.collect());
    _regions[0] = {p.first, p.second, _left.get_pixmap()};
    p = _win.update_right(_right.collect());
    _regions[1] = {p.first, p.second, _right.get_pixmap()};
    p = _win.update_middle(_middle.collect());
    _regions[2] = {p.first, p.second, _middle.get_pixmap()};

    _win.render();

    _work.pop();
  }
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
