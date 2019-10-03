// vim:sw=2:ts=2:et:

/** TODO
 * - Modules should return pixmaps or a format that does not need to be parsed
 *   but is instead sent directly to the bar.
 * - Find more ergonomic way to reference singletons.
 * - Use static polymorphism with modules.
 * - Initial bar display is really buggy.
 * - Remove all init functions in favor of constructors.
 * - Can the call to system be avoided with direct calls to X instead?
 */

#include "color.h"
#include "config.h"
#include "enums.h"
#include "fonts.h"
#include "monitors.h"
#include "x.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <xcb/randr.h>

static Fonts fonts;

void
parse (char *text) {
  int pos_x = 0, align = ALIGN_L;
  int button;
  char *p = text, *block_end, *ep;

  auto mon_itr = Monitors::Instance()->begin();

  Monitors::Instance()->_areas.clear();

  for (const monitor_t& m : *Monitors::Instance())
    X::Instance()->fill_rect(m._pixmap, GC_CLEAR, 0, 0, m._width, BAR_HEIGHT);

  /* Create xft drawable */
  if (!(X::Instance()->xft_draw = (X::Instance()->xft_draw_create(mon_itr->_pixmap)))) {
    fprintf(stderr, "Couldn't create xft drawable\n");
  }

  for (;;) {
    if (*p == '\0' || *p == '\n')
      break;

    if (p[0] == '%' && p[1] == '{' && (block_end = strchr(p++, '}'))) {
      p++;
      while (p < block_end) {
        int w;
        while (isspace(*p))
          p++;

        switch (*p++) {
          case '+': set_attribute('+', *p++); break;
          case '-': set_attribute('-', *p++); break;
          case '!': set_attribute('!', *p++); break;

          case 'l': pos_x = 0; align = ALIGN_L; break;
          case 'c': pos_x = 0; align = ALIGN_C; break;
          case 'r': pos_x = 0; align = ALIGN_R; break;

          case 'A':
            button = XCB_BUTTON_INDEX_1;
            // The range is 1-5
            if (isdigit(*p) && (*p > '0' && *p < '6'))
              button = *p++ - '0';
            if (!Monitors::Instance()->area_add(p, block_end, &p, &*mon_itr, pos_x, align, button))
              return;
            break;

          case 'B': X::Instance()->bgc = rgba_t::parse(p, &p); X::Instance()->update_gc(); break;
          case 'F': X::Instance()->fgc = rgba_t::parse(p, &p); X::Instance()->update_gc(); break;
          case 'U': X::Instance()->ugc = rgba_t::parse(p, &p); X::Instance()->update_gc(); break;

          case 'S':
            if (isdigit(*p)) {
              mon_itr = Monitors::Instance()->begin() + *p-'0';
            } else {
              p++;
              continue;
            }

            XftDrawDestroy (X::Instance()->xft_draw);
            if (!(X::Instance()->xft_draw = X::Instance()->xft_draw_create(mon_itr->_pixmap))) {
              fprintf(stderr, "Couldn't create xft drawable\n");
            }

            p++;
            pos_x = 0;
            break;
          case 'O':
            errno = 0;
            w = (int) strtoul(p, &p, 10);
            if (errno)
              continue;

            mon_itr->draw_shift(pos_x, align, w);

            pos_x += w;
            Monitors::Instance()->area_shift(mon_itr->_window, align, w);
            break;

          case 'T':
            if (*p == '-') { //Reset to automatic font selection
              fonts._index = -1;
              p++;
              break;
            } else if (isdigit(*p)) {
              fonts._index = (int)strtoul(p, &ep, 10);
              // User-specified 'font_index' ∊ (0,font_count]
              // Otherwise just fallback to the automatic font selection
              if (fonts._index < 0 || fonts._index > FONTS.size())
                fonts._index = -1;
              p = ep;
              break;
            } else {
              fprintf(stderr, "Invalid font slot \"%c\"\n", *p++); //Swallow the token
              break;
            }

            // In case of error keep parsing after the closing }
          default:
            p = block_end;
        }
      }
      ++p; // Eat the trailing }
    } else { // utf-8 -> ucs-2
      uint8_t *utf = (uint8_t *)p;
      uint16_t ucs;

      // ASCII
      if (utf[0] < 0x80) {
        ucs = utf[0];
        p  += 1;
      }
      // Two byte utf8 sequence
      else if ((utf[0] & 0xe0) == 0xc0) {
        ucs = (utf[0] & 0x1f) << 6 | (utf[1] & 0x3f);
        p += 2;
      }
      // Three byte utf8 sequence
      else if ((utf[0] & 0xf0) == 0xe0) {
        ucs = (utf[0] & 0xf) << 12 | (utf[1] & 0x3f) << 6 | (utf[2] & 0x3f);
        p += 3;
      }
      // Four byte utf8 sequence
      else if ((utf[0] & 0xf8) == 0xf0) {
        ucs = 0xfffd;
        p += 4;
      }
      // Five byte utf8 sequence
      else if ((utf[0] & 0xfc) == 0xf8) {
        ucs = 0xfffd;
        p += 5;
      }
      // Six byte utf8 sequence
      else if ((utf[0] & 0xfe) == 0xfc) {
        ucs = 0xfffd;
        p += 6;
      }
      // Not a valid utf-8 sequence
      else {
        ucs = utf[0];
        p += 1;
      }

      font_t *cur_font = fonts.select_drawable_font(ucs);
      if (!cur_font)
        continue;

      int w = mon_itr->draw_char(cur_font, pos_x, align, ucs);

      pos_x += w;
      Monitors::Instance()->area_shift(mon_itr->_window, align, w);
    }
  }
  XftDrawDestroy (X::Instance()->xft_draw);
}

void
set_ewmh_atoms () {
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
  std::array<xcb_intern_atom_cookie_t, size> atom_cookies;
  std::array<xcb_atom_t, size> atom_list;
  xcb_intern_atom_reply_t *atom_reply;

  // As suggested fetch all the cookies first (yum!) and then retrieve the
  // atoms to exploit the async'ness
  std::transform(atom_names.begin(), atom_names.end(), atom_cookies.begin(), [](auto name){
    return xcb_intern_atom(X::Instance()->get_connection(), 0, strlen(name), name);
  });

  for (int i = 0; i < atom_names.size(); i++) {
    atom_reply = xcb_intern_atom_reply(X::Instance()->get_connection(), atom_cookies[i], nullptr);
    if (!atom_reply)
      return;
    atom_list[i] = atom_reply->atom;
    free(atom_reply);
  }

  // Prepare the strut array
  for (const auto& mon : *Monitors::Instance()) {
    int strut[12] = {0};
    if (TOPBAR) {
      strut[2] = BAR_HEIGHT;
      strut[8] = mon._x;
      strut[9] = mon._x + mon._width;
    } else {
      strut[3]  = BAR_HEIGHT;
      strut[10] = mon._x;
      strut[11] = mon._x + mon._width;
    }

    X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window, atom_list[NET_WM_WINDOW_TYPE], XCB_ATOM_ATOM, 32, 1, &atom_list[NET_WM_WINDOW_TYPE_DOCK]);
    X::Instance()->change_property(XCB_PROP_MODE_APPEND,  mon._window, atom_list[NET_WM_STATE], XCB_ATOM_ATOM, 32, 2, &atom_list[NET_WM_STATE_STICKY]);
    X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window, atom_list[NET_WM_DESKTOP], XCB_ATOM_CARDINAL, 32, 1, (const uint32_t []) { 0u - 1u } );
    X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window, atom_list[NET_WM_STRUT_PARTIAL], XCB_ATOM_CARDINAL, 32, 12, strut);
    X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window, atom_list[NET_WM_STRUT], XCB_ATOM_CARDINAL, 32, 4, strut);
    X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 3, "bar");
    X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, 12, "lemonbar\0Bar");
  }
}

void
module_events() {
  char input[4096] = {0, };
  while (true) {
    // If connection is in error state, then it has been shut down.
    if (xcb_connection_has_error(X::Instance()->get_connection()))
      break;

    // a module has changed and the bar needs to be redrawn
    {
      std::mutex mutex;
      std::unique_lock<std::mutex> lock(mutex);
      Module::condvar.wait(lock);

      std::stringstream ss;
      ss << "%{l} ";
      ss << modules.find("workspaces")->second->get();
      ss << " ";
      ss << modules.find("windows")->second->get();
      ss << "%{c}";
      ss << modules.find("clock")->second->get();
      ss << "%{r}";

      std::stringstream full_bar;
      std::string bar_str(ss.str());
      for (int i = 0; i < Monitors::Instance()->_monitors.size(); ++i) {
        full_bar << "%{S" << i << "}" << bar_str;
      }
      std::string full_bar_str(full_bar.str());

      strncpy(input, full_bar_str.c_str(), full_bar_str.size());
      input[full_bar_str.size()] = '\0';
      parse(input);
    }

    for (const auto& mon : *Monitors::Instance()) {
      X::Instance()->copy_area(mon._pixmap, mon._window, 0, 0, mon._width);
    }
    X::Instance()->flush();
  }
}

void
bar_events() {
  while (true) {
    bool redraw = false;

    // If connection is in error state, then it has been shut down.
    if (xcb_connection_has_error(X::Instance()->get_connection()))
      break;

    // handle bar related events
    for (xcb_generic_event_t *ev; (ev = xcb_wait_for_event(X::Instance()->get_connection())); free(ev)) {
      switch (ev->response_type & 0x7F) {
        case XCB_EXPOSE:
          redraw = reinterpret_cast<xcb_expose_event_t*>(ev)->count == 0;
          break;
        case XCB_BUTTON_PRESS:
          auto *press_ev = reinterpret_cast<xcb_button_press_event_t*>(ev);
          auto area = Monitors::Instance()->area_get(press_ev->event, press_ev->detail, press_ev->event_x);
          if (area) system(area->cmd);
          break;
      }
    }

    if (redraw) { // Copy our temporary pixmap onto the window
      for (const auto& mon : *Monitors::Instance()) {
        X::Instance()->copy_area(mon._pixmap, mon._window, 0, 0, mon._width);
      }
    }

    X::Instance()->flush();
  }

}

int
main ()
{
  fonts.init(X::Instance()->get_connection(), 0);

  // For WM that support EWMH atoms
  set_ewmh_atoms();

  // Make the bar visible and clear the pixmap
  for (const auto& mon : *Monitors::Instance()) {
    X::Instance()->fill_rect(mon._pixmap, GC_CLEAR, 0, 0, mon._width, BAR_HEIGHT);
    xcb_map_window(X::Instance()->get_connection(), mon._window);

    // Make sure that the window really gets in the place it's supposed to be
    // Some WM such as Openbox need this
    const uint32_t xy[] { static_cast<uint32_t>(mon._x), static_cast<uint32_t>(mon._y) };
    xcb_configure_window(X::Instance()->get_connection(), mon._window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, xy);

    // Set the WM_NAME atom to the user specified value
    if constexpr (WM_NAME != nullptr)
      X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window,
          XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(WM_NAME), WM_NAME);

    // set the WM_CLASS atom instance to the executable name
    if (WM_CLASS.size()) {
      constexpr int size = WM_CLASS.size() + 6;
      char wm_class[size] = {0};

      // WM_CLASS is nullbyte seperated: WM_CLASS + "\0Bar\0"
      strncpy(wm_class, WM_CLASS.data(), WM_CLASS.size());
      strcpy(wm_class + WM_CLASS.size(), "\0Bar");

      X::Instance()->change_property(XCB_PROP_MODE_REPLACE, mon._window,
          XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, size, wm_class);
    }
  }

  char color[] = "#ffffff";
  uint32_t nfgc = *X::Instance()->fgc.val() & 0x00ffffff;
  snprintf(color, sizeof(color), "#%06X", nfgc);

  if (!X::Instance()->xft_color_alloc_name(color)) {
    fprintf(stderr, "Couldn't allocate xft font color '%s'\n", color);
  }
  X::Instance()->flush();

  std::vector<std::thread> threads;
  for (const auto& mod : modules) {
    threads.emplace_back(std::ref(*mod.second));
  }
  threads.emplace_back(bar_events);
  threads.emplace_back(module_events);

  for (std::thread& thread : threads) {
    thread.join();
  }

  return EXIT_SUCCESS;
}
