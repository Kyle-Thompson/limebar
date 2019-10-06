#pragma once

#include "module.h"

#include <array>

template <typename... T>
class ModuleContainer {
 public:
  constexpr ModuleContainer() {}

// private:
  std::tuple<Module<T>...> _modules;
};
