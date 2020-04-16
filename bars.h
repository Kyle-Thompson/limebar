#pragma once

#include <algorithm>
#include <array>
#include <cstddef>  // size_t
#include <memory>
#include <tuple>
#include <utility>  // pair

#include "bar_color.h"
#include "modules/module.h"
#include "pixmap.h"
#include "window.h"


/** Section
 * A bar is broken up into three sections; left, middle, and right. This class
 * stores the modules in a given section and collects their current values into
 * one representation of the state of the section at the time which it was
 * called.
 */
template <typename... Mods>
class Section {
 public:
  Section(padding_t padding, BarWindow* win, std::tuple<const Mods&...> mods);

  const SectionPixmap& collect();

  SectionPixmap* get_pixmap() { return &_pixmap; }

 private:
  // TODO have a text_segment_t divider
  padding_t _padding;
  SectionPixmap _pixmap;
  std::tuple<const Mods&...> _modules;
};


template <typename... Mods>
Section<Mods...>::Section(padding_t padding, BarWindow* win,
                          std::tuple<const Mods&...> mods)
    : _padding(padding), _pixmap(win->generate_mod_pixmap()), _modules(mods) {
}


template <typename... Mods>
const SectionPixmap&
Section<Mods...>::collect() {
  _pixmap.clear();
  _pixmap.pad(_padding.start);
  std::apply(
      [this, i = 0](const auto&... mod) {
        (([&] {
           for (const auto& m : mod.get()) {
             _pixmap.write(m, _padding.intra_module);
           }
           if (i < sizeof...(mod) - 1) {
             _pixmap.pad(_padding.inter_module);
           }
         }()),
         ...);
      },
      _modules);
  _pixmap.pad(_padding.end);
  return _pixmap;
}


template <typename L, typename M, typename R>
class BarBuilder;


/** Bar
 * The Bar class maintains the three different sections and the window
 * displaying the bar itself. It will also draw each section into the bar.
 */
template <typename L, typename M, typename R>
class Bar;

template <typename... Left, typename... Middle, typename... Right>
class Bar<std::tuple<const Left&...>, std::tuple<const Middle&...>,
          std::tuple<const Right&...>> {
 public:
  class events_t;

  explicit Bar(
      const BarBuilder<std::tuple<const Left&...>, std::tuple<const Middle&...>,
                       std::tuple<const Right&...>>& builder);

  void update();
  void click(int16_t x, uint8_t button) const;
  auto get_event_handler() -> events_t* { return &_events; }

 private:
  BarWindow _win;
  events_t _events;
  Section<const Left&...> _left;
  Section<const Middle&...> _middle;
  Section<const Right&...> _right;
  std::array<std::tuple<int16_t, int16_t, SectionPixmap*>, 3> _regions;
};


// template deduction guide
template <typename... Left, typename... Middle, typename... Right>
Bar(const BarBuilder<std::tuple<const Left&...>, std::tuple<const Middle&...>,
                     std::tuple<const Right&...>>& builder)
    -> Bar<std::tuple<const Left&...>, std::tuple<const Middle&...>,
           std::tuple<const Right&...>>;


template <typename... Left, typename... Middle, typename... Right>
Bar<std::tuple<const Left&...>, std::tuple<const Middle&...>,
    std::tuple<const Right&...>>::
    Bar(const BarBuilder<std::tuple<const Left&...>,
                         std::tuple<const Middle&...>,
                         std::tuple<const Right&...>>& builder)
    : _win([&builder]() -> BarWindow {
      auto& ds = DS::Instance();
      auto rdb = ds.create_resource_database();

      auto fg = ds.create_font_color(rgba_t::parse(
          builder._font_fg.from_rdb
              ? rdb.get<std::string>(builder._font_fg.name).c_str()
              : builder._font_fg.name));
      auto acc = ds.create_font_color(rgba_t::parse(
          builder._font_acc.from_rdb
              ? rdb.get<std::string>(builder._font_acc.name).c_str()
              : builder._font_acc.name));

      BarColors bar_colors{
          .background =
              rgba_t::parse(builder._bg.from_rdb
                                ? rdb.get<std::string>(builder._bg.name).c_str()
                                : builder._bg.name),
          .foreground = fg,
          .fg_accent = acc};

      return BarWindow(std::move(bar_colors), builder._rect);
    }())
    , _events(this)
    , _left(builder._padding, &_win, builder._left)
    , _middle(builder._padding, &_win, builder._middle)
    , _right(builder._padding, &_win, builder._right) {
}


template <typename... Left, typename... Middle, typename... Right>
void
Bar<std::tuple<const Left&...>, std::tuple<const Middle&...>,
    std::tuple<const Right&...>>::update() {
  _win.reset();

  std::pair<uint16_t, uint16_t> p;
  p = _win.update_left(_left.collect());
  _regions[0] = {p.first, p.second, _left.get_pixmap()};
  p = _win.update_right(_right.collect());
  _regions[1] = {p.first, p.second, _right.get_pixmap()};
  p = _win.update_middle(_middle.collect());
  _regions[2] = {p.first, p.second, _middle.get_pixmap()};

  _win.render();
}


template <typename... Left, typename... Middle, typename... Right>
void
Bar<std::tuple<const Left&...>, std::tuple<const Middle&...>,
    std::tuple<const Right&...>>::click(int16_t x, uint8_t button) const {
  for (auto region : _regions) {
    if (x >= std::get<0>(region) && x <= std::get<1>(region)) {
      std::get<2>(region)->click(x - std::get<0>(region), button);
      break;
    }
  }
}


/** events_t
 * A taskable class to handle user interactions with the bar.
 */
template <typename... Left, typename... Middle, typename... Right>
class Bar<std::tuple<const Left&...>, std::tuple<const Middle&...>,
          std::tuple<const Right&...>>::events_t {
 public:
  explicit events_t(Bar* bar) : _bar(bar), _ds(DS::Instance()) {}
  bool has_work();
  void do_work() { _bar->click(_event_x, _event_button); }

 private:
  Bar* _bar;
  DS& _ds;
  int16_t _event_x{0};
  uint8_t _event_button{0};
};

template <typename... Left, typename... Middle, typename... Right>
bool
Bar<std::tuple<const Left&...>, std::tuple<const Middle&...>,
    std::tuple<const Right&...>>::events_t::has_work() {
  while (auto event = _ds.poll_for_event()) {
    // TODO: can we filter in the display server to only return these values
    // in the first place so we don't have to check every time?
    if ((event->response_type & 0x7F) == XCB_BUTTON_PRESS) {
      auto* press = reinterpret_cast<xcb_button_press_event_t*>(event.get());
      _event_x = press->event_x;
      _event_button = press->detail;
      return true;
    }
  }
  return false;
}


struct lookup_value_t {
  const char* name = nullptr;
  bool from_rdb = false;
};


/** BarBuilder
 * A helper class to build Bars in a declarative style.
 */
template <typename... L, typename... M, typename... R>
class BarBuilder<std::tuple<const L&...>, std::tuple<const M&...>,
                 std::tuple<const R&...>> {
 public:
  consteval BarBuilder() = default;

  consteval auto area(rectangle_t rect) const;
  consteval auto padding(padding_t padding) const;

  consteval auto bg_bar_color(const char* str) const;
  consteval auto bg_bar_color_from_rdb(const char* str) const;

  consteval auto fg_font_color(const char* str) const;
  consteval auto fg_font_color_from_rdb(const char* str) const;

  consteval auto acc_font_color(const char* str) const;
  consteval auto acc_font_color_from_rdb(const char* str) const;

  template <typename... Mods>
  consteval auto left(const Mods&... tup) const;

  template <typename... Mods>
  consteval auto middle(const Mods&... tup) const;

  template <typename... Mods>
  consteval auto right(const Mods&... tup) const;

 private:
  friend class Bar<std::tuple<const L&...>, std::tuple<const M&...>,
                   std::tuple<const R&...>>;

  rectangle_t _rect;
  padding_t _padding;
  lookup_value_t _bg;
  lookup_value_t _font_fg;
  lookup_value_t _font_acc;
  std::tuple<const L&...> _left;
  std::tuple<const M&...> _middle;
  std::tuple<const R&...> _right;

 public:
  consteval explicit BarBuilder(rectangle_t rect, padding_t padding,
                                lookup_value_t bg, lookup_value_t font_fg,
                                lookup_value_t font_acc,
                                std::tuple<const L&...> left,
                                std::tuple<const M&...> middle,
                                std::tuple<const R&...> right);
};


// template deduction guide
// TODO: this currently doesn't work
/* template <typename... L, typename... M, typename... R> */
/* BarBuilder(rectangle_t rect, padding_t padding, lookup_value_t bg, */
/*            lookup_value_t font_fg, lookup_value_t font_acc, */
/*            std::tuple<const L&...> left, std::tuple<const M&...> middle, */
/*            std::tuple<const R&...> right) */
/*     -> BarBuilder<std::tuple<const L&...>, std::tuple<const M&...>, */
/*                   std::tuple<const R&...>>; */

using BarBuilderHelper = BarBuilder<std::tuple<>, std::tuple<>, std::tuple<>>;


template <typename... L, typename... M, typename... R>
consteval BarBuilder<
    std::tuple<const L&...>, std::tuple<const M&...>,
    std::tuple<const R&...>>::BarBuilder(rectangle_t rect, padding_t padding,
                                         lookup_value_t bg,
                                         lookup_value_t font_fg,
                                         lookup_value_t font_acc,
                                         std::tuple<const L&...> left,
                                         std::tuple<const M&...> middle,
                                         std::tuple<const R&...> right)
    : _rect(rect)
    , _padding(padding)
    , _bg(bg)
    , _font_fg(font_fg)
    , _font_acc(font_acc)
    , _left(left)
    , _middle(middle)
    , _right(right) {
}


#define BAR_BUILDER_FUNC                                                      \
  template <typename... L, typename... M, typename... R>                      \
  consteval auto BarBuilder<std::tuple<const L&...>, std::tuple<const M&...>, \
                            std::tuple<const R&...>>

BAR_BUILDER_FUNC::area(rectangle_t rect) const {
  return BarBuilder(rect, _padding, _bg, _font_fg, _font_acc, _left, _middle,
                    _right);
}

BAR_BUILDER_FUNC::padding(padding_t padding) const {
  return BarBuilder(_rect, padding, _bg, _font_fg, _font_acc, _left, _middle,
                    _right);
}

BAR_BUILDER_FUNC::bg_bar_color(const char* str) const {
  return BarBuilder(_rect, _padding, {.name = str, .from_rdb = false}, _font_fg,
                    _font_acc, _left, _middle, _right);
}

BAR_BUILDER_FUNC::bg_bar_color_from_rdb(const char* str) const {
  return BarBuilder(_rect, _padding, {.name = str, .from_rdb = true}, _font_fg,
                    _font_acc, _left, _middle, _right);
}

BAR_BUILDER_FUNC::fg_font_color(const char* str) const {
  return BarBuilder(_rect, _padding, _bg, {.name = str, .from_rdb = false},
                    _font_acc, _left, _middle, _right);
}

BAR_BUILDER_FUNC::fg_font_color_from_rdb(const char* str) const {
  return BarBuilder(_rect, _padding, _bg, {.name = str, .from_rdb = true},
                    _font_acc, _left, _middle, _right);
}

BAR_BUILDER_FUNC::acc_font_color(const char* str) const {
  return BarBuilder(_rect, _padding, _bg, _font_fg,
                    {.name = str, .from_rdb = false}, _left, _middle, _right);
}

BAR_BUILDER_FUNC::acc_font_color_from_rdb(const char* str) const {
  return BarBuilder(_rect, _padding, _bg, _font_fg,
                    {.name = str, .from_rdb = true}, _left, _middle, _right);
}


#define BAR_BUILDER_SECTION_FUNC                                              \
  template <typename... L, typename... M, typename... R>                      \
  template <typename... Mods>                                                 \
  consteval auto BarBuilder<std::tuple<const L&...>, std::tuple<const M&...>, \
                            std::tuple<const R&...>>

BAR_BUILDER_SECTION_FUNC::left(const Mods&... tup) const {
  return BarBuilder<std::tuple<const Mods&...>, std::tuple<const M&...>,
                    std::tuple<const R&...>>(
      _rect, _padding, _bg, _font_fg, _font_acc, {tup...}, _middle, _right);
}

BAR_BUILDER_SECTION_FUNC::middle(const Mods&... tup) const {
  return BarBuilder<std::tuple<const L&...>, std::tuple<const Mods&...>,
                    std::tuple<const R&...>>(
      _rect, _padding, _bg, _font_fg, _font_acc, _left, {tup...}, _right);
}

BAR_BUILDER_SECTION_FUNC::right(const Mods&... tup) const {
  return BarBuilder<std::tuple<const L&...>, std::tuple<const M&...>,
                    std::tuple<const Mods&...>>(
      _rect, _padding, _bg, _font_fg, _font_acc, _left, _middle, {tup...});
}
