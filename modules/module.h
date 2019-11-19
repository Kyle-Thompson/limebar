#pragma once

#include "../window.h"
#include "../x.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <utility>
#include <xcb/xproto.h>

static std::condition_variable condvar;

struct Area {
  uint16_t begin, end;
  std::function<void(uint8_t button)> action;
};

template <typename Mod>
class Module {
 public:
  Module(const BarWindow& win)
    : _pixmap(win.generate_mod_pixmap())
  {}

  std::pair<ModulePixmap&, std::unique_lock<std::mutex>> get() {
    return std::pair<ModulePixmap&,
                     std::unique_lock<std::mutex>>{std::ref(_pixmap), _mutex};
  }

  void operator()[[noreturn]] () {
    update();
    while (true) {
      static_cast<Mod&>(*this).trigger();
      update();
    }
  }

 protected:
  void update() {
    std::lock_guard<std::mutex> g(_mutex);
    _pixmap.clear();
    static_cast<Mod&>(*this).update();
    condvar.notify_one();
  }

  ModulePixmap _pixmap;
  std::mutex _mutex;
  /* std::array<Area, Mod::MAX_AREAS> _areas; */
};
