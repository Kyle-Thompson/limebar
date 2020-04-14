#include "color.h"

#include <iostream>

rgba_t
rgba_t::parse(const char *str) {
  static const rgba_t ERR_COLOR{0xffffffffU};

  if (str == nullptr) {
    return ERR_COLOR;
  }

  // Hex representation
  if (str[0] != '#') {
    std::cerr << "Invalid color specified\n";
    return ERR_COLOR;
  }

  char *ep = nullptr;
  rgba_t tmp{static_cast<uint32_t>(strtoul(str + 1, &ep, 16))};

  int string_len = ep - (str + 1);
  switch (string_len) {
    case 3:
      // Expand the #rgb format into #rrggbb (aa is set to 0xff)
      tmp.set((*tmp.val() & 0xf00) * 0x1100 | (*tmp.val() & 0x0f0) * 0x0110 |
              (*tmp.val() & 0x00f) * 0x0011);
      [[fallthrough]];
    case 6:
      // If the code is in #rrggbb form then assume it's opaque
      tmp.a = 255;
      break;
    case 7:
    case 8:
      // Colors in #aarrggbb format, those need no adjustments
      break;
    default:
      std::cerr << "Invalid color specified\n";
      return ERR_COLOR;
  }

  // Premultiply the alpha in
  if (tmp.a) {
    // The components are clamped automagically as the rgba_t is made of uint8_t
    return {
        static_cast<uint8_t>((tmp.r * tmp.a) / 255),
        static_cast<uint8_t>((tmp.g * tmp.a) / 255),
        static_cast<uint8_t>((tmp.b * tmp.a) / 255),
        tmp.a,
    };
  }

  return rgba_t{0U};
}
