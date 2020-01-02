#pragma once

#include <bits/stdint-uintn.h>
#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <vector>

template <typename DS>
class Fonts {
 public:
  using Font = typename DS::font_t;

  Fonts(std::initializer_list<Font *> fonts)
    :_fonts(fonts)
  {
    // to make the alignment uniform, find maximum height
    const int maxh = (*std::max_element(_fonts.begin(), _fonts.end(),
        [](const auto& l, const auto& r){
          return l->height < r->height;
        }))->height;

    // set maximum height to all fonts
    for (auto& font : _fonts) {
      font->height = maxh;
    }
  }

  Font* operator[](size_t index) { return _fonts[index]; }
  Font* drawable_font(uint16_t ch) {
    auto itr = _chars.find(ch);
    return (itr == _chars.end() ? add_char(ch) : itr)->second;
  }

 private:
  auto add_char(uint16_t ch) {
    Font *font = [ch, this] {
      for (auto* font : _fonts) {
        if (font->has_glyph(ch)) {
          return font;
        }
      }
      std::cerr << "error: character " << ch << " could not be found.\n";
      return _fonts[0];  // TODO: print error and exit?
    }();
    return _chars.emplace(std::make_pair(ch, font)).first;
  }

  std::vector<Font*> _fonts;
  // map from ucs2 string to corresponding font
  // TODO: use a better hashmap
  std::unordered_map<uint16_t, Font*> _chars;
};
