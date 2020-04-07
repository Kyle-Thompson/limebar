#pragma once

#include <algorithm>
#include <array>
#include <cstddef>  // size_t
#include <memory>
#include <tuple>
#include <utility>  // pair

#include "bar_color.h"
#include "font.h"
#include "modules/module.h"
#include "pixmap.h"
#include "window.h"


template <typename... Mods>
std::tuple<const Mods&...>
section_wrapper(const Mods&... mods) {
  return {mods...};
}


/** Section
 * A bar is broken up into three sections; left, middle, and right. This class
 * stores the modules in a given section and collects their current values into
 * one representation of the state of the section at the time which it was
 * called.
 *
 * TODO: how to verify that Mods is a tuple?
 */
template <typename... Mods>
class Section {
 public:
  Section(BarWindow* win, const std::tuple<const Mods&...>& mods);

  const SectionPixmap& collect();

  SectionPixmap* get_pixmap() { return &_pixmap; }

 private:
  SectionPixmap _pixmap;
  const std::tuple<const Mods&...>& _modules;
};

template <typename... Mods>
Section<Mods...>::Section(BarWindow* win,
                          const std::tuple<const Mods&...>& mods)
    : _pixmap(win->generate_mod_pixmap()), _modules(mods) {
}

template <typename... Mods>
const SectionPixmap&
Section<Mods...>::collect() {
  _pixmap.clear();
  std::apply([this](const auto&... mod) { ((mod.get() | _pixmap), ...); },
             _modules);
  return _pixmap;
}


/** Bar
 * The Bar class maintains the three different sections and the window
 * displaying the bar itself. It will also draw each section into the bar.
 */
template <typename L, typename M, typename R>
class Bar;

template <typename... Left, typename... Middle, typename... Right>
class Bar<std::tuple<Left...>, std::tuple<Middle...>, std::tuple<Right...>> {
 public:
  Bar(rectangle_t&& d, BarColors&& colors, Fonts&& fonts,
      const std::tuple<const Left&...>& left,
      const std::tuple<const Middle&...>& middle,
      const std::tuple<const Right&...>& right);

  void operator()();
  template <typename Mod>
  void use(const DynamicModule<Mod>& mod);
  void click(uint16_t x, uint8_t button) const {
    for (auto region : _regions) {
      if (x >= std::get<0>(region) && x <= std::get<1>(region)) {
        std::get<2>(region)->click(x - std::get<0>(region), button);
        break;
      }
    }
  }

  class events_t {
   public:
    explicit events_t(Bar* bar) : _bar(bar), _ds(DS::Instance()) {}

    bool has_work() {
      while (auto event = _ds.poll_for_event()) {
        // TODO: can we filter in the display server to only return these values
        // in the first place so we don't have to check every time?
        if ((event->response_type & 0x7F) == XCB_BUTTON_PRESS) {
          auto* press =
              reinterpret_cast<xcb_button_press_event_t*>(event.get());
          _event_x = press->event_x;
          _event_button = press->detail;
          return true;
        }
      }
      return false;
    }

    void do_work() { _bar->click(_event_x, _event_button); }

   private:
    Bar* _bar;
    DS& _ds;
    uint16_t _event_x;
    uint8_t _event_button;
  };

  events_t* get_event_handler() { return &_events; }

 private:
  BarWindow _win;
  events_t _events;
  Section<Left...> _left;
  Section<Middle...> _middle;
  Section<Right...> _right;
  std::array<std::tuple<size_t, size_t, SectionPixmap*>, 3> _regions;
};

template <typename... Left, typename... Middle, typename... Right>
Bar<std::tuple<Left...>, std::tuple<Middle...>, std::tuple<Right...>>::Bar(
    rectangle_t&& d, BarColors&& colors, Fonts&& fonts,
    const std::tuple<const Left&...>& left,
    const std::tuple<const Middle&...>& middle,
    const std::tuple<const Right&...>& right)
    : _win(std::move(colors), std::move(fonts), d)
    , _events(this)
    , _left(&_win, left)
    , _middle(&_win, middle)
    , _right(&_win, right) {
}

template <typename... Left, typename... Middle, typename... Right>
template <typename Mod>
void
Bar<std::tuple<Left...>, std::tuple<Middle...>, std::tuple<Right...>>::use(
    const DynamicModule<Mod>& mod) {
  _win.reset();

  std::pair<size_t, size_t> p;
  p = _win.update_left(_left.collect());
  _regions[0] = {p.first, p.second, _left.get_pixmap()};
  p = _win.update_right(_right.collect());
  _regions[1] = {p.first, p.second, _right.get_pixmap()};
  p = _win.update_middle(_middle.collect());
  _regions[2] = {p.first, p.second, _middle.get_pixmap()};

  _win.render();
}
