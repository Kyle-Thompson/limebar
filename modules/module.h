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

 protected:
  ModulePixmap _pixmap;
  /* std::array<Area, Mod::MAX_AREAS> _areas; */
};
