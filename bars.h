#pragma once

#include "modules/module.h"
#include "pixmap.h"
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


/** section_t
 * A bar is broken up into three sections; left, middle, and right. This class
 * stores the modules in a given section, manages their threads and collects
 * their current values into one representation of the state of the section at
 * the time which it was called.
 */
template <typename ...Mod>
struct section_t {
  section_t(const BarWindow& win);
  // TODO: collect generator to return one module pixmap and its lock at a time.

  std::tuple<Mod...> _modules;
  std::array<std::thread, sizeof...(Mod)> _module_threads;
};

template <typename ...Mod>
section_t<Mod...>::section_t(const BarWindow& win)
  : _modules( ((void)sizeof(Mod), win)... )
  , _module_threads(
      [this]<size_t... I>(std::index_sequence<I...>) -> decltype(_module_threads) {
        return { std::thread(std::ref(std::get<I>(_modules)))... };
      }(std::make_index_sequence<sizeof...(Mod)>{}))
{}


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
  void update();

  /* std::condition_variable _condvar;  // TODO: Each bar should have it's own synchronization. */
  BarWindow _win;
  std::tuple<Left, Middle, Right> _sections;
};

template <size_t origin_x, size_t origin_y,
          size_t width, size_t height,
          typename Left, typename Middle, typename Right>
bar_t<origin_x, origin_y, width, height, Left, Middle, Right>::bar_t()
  : _win(origin_x, origin_y, width, height)
  , _sections(_win, _win, _win)
{}

template <size_t origin_x, size_t origin_y,
          size_t width, size_t height,
          typename Left, typename Middle, typename Right>
void
bar_t<origin_x, origin_y, width, height, Left, Middle, Right>::operator()()
{

  while (true) {
    _win.clear();
    update();
    _win.render();

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
  // TODO: update with section collect when available. This is so gross.
  std::apply(
      [&](auto&... mod) {
        ([&mod, this]{
           const auto& pack = mod.get();
           _win.update_left(pack.pixmap);
         }(), ...);
      },
      std::get<0>(_sections)._modules);


  // add right side modules
  std::apply(
      [&](auto&... mod) {
        ([&mod, this]{
           const auto& pack = mod.get();
           _win.update_right(pack.pixmap);
         }(), ...);
      },
      std::get<2>(_sections)._modules);


  // add middle modules
  auto mid_pix = _win.generate_mod_pixmap();
  std::apply(
      [&](auto&... mod) {
        ([&/*, this*/]{
           const auto& pack = mod.get();
           /* _win.update_middle(pack.pixmap);  // TODO(1): uncomment */
           mid_pix.append(pack.pixmap);
         }(), ...);
      },
      std::get<1>(_sections)._modules);
  _win.update_middle(mid_pix);  // TODO(1): delete
}


/** Bars
 * Run each bar in their own thread.
 */
template <typename ...Bar>
struct Bars {
  Bars()
    /* : _threads(std::thread(Bar{})...) */
    : _threads(
        [this]<size_t... I>(std::index_sequence<I...>) -> decltype(_threads) {
          return { std::thread(std::ref(std::get<I>(_bars)))... };
        }(std::make_index_sequence<sizeof...(Bar)>{}))
  {}
  ~Bars() {
    for (auto& t : _threads)
      t.join();
  }

  // TODO: have threads own each bar.
  std::tuple<Bar...> _bars;
  // TODO: jthreads
  std::array<std::thread, sizeof...(Bar)> _threads;
};
