#pragma once

/** TODO
 * can we avoid spawning a thread for static modules?
 */

#include "module.h"

template <typename Mod>
class StaticModule : public Module<StaticModule<Mod>> {
  using PARENT = Module<StaticModule<Mod>>;

 public:
  struct ModPack {
    ModulePixmap& pixmap;
  };

  StaticModule(const BarWindow& win) : Module<StaticModule<Mod>>(win) {}
  void operator()() {}

  ModPack get() {
    return {
      .pixmap = std::ref(PARENT::_pixmap),
    };
  }
};
