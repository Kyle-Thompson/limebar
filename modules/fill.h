#pragma once

#include "static_module.h"
#include <array>

// TODO: replace with std::string when that becomes standardized
template <char... Chars>
class mod_fill : public StaticModule<mod_fill<Chars...>> {
  using PARENT = StaticModule<mod_fill<Chars...>>;

 public:
  mod_fill(const BarWindow& win)
    : PARENT(win)
  {
    std::array<char, sizeof...(Chars) + 1> chars { Chars... };
    chars[sizeof...(Chars)] = '\0';
    PARENT::_pixmap.write(chars.data());
  }
};
