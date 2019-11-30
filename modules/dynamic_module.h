#pragma once

#include "module.h"

template <typename Mod>
class DynamicModule : public Module<DynamicModule<Mod>> {
  using PARENT = Module<DynamicModule<Mod>>;

 public:
  struct ModPack {
    ModulePixmap& pixmap;
    std::unique_lock<std::mutex> lock;
  };

  DynamicModule(const BarWindow& win) : Module<DynamicModule<Mod>>(win) {}

  void operator()[[noreturn]]();

  ModPack get() {
    return {
      .pixmap = std::ref(PARENT::_pixmap),
      .lock   = std::unique_lock(_mutex),
    };
  }

 protected:
  void update();

  std::mutex _mutex;
};

template <typename Mod>
void
DynamicModule<Mod>::operator()() {
  update();
  while (true) {
    static_cast<Mod&>(*this).trigger();
    update();
  }
}

template <typename Mod>
void
DynamicModule<Mod>::update() {
  std::lock_guard<std::mutex> g(_mutex);
  PARENT::_pixmap.clear();
  static_cast<Mod&>(*this).update();
  condvar.notify_one();
}
