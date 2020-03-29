#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "config.h"
#include "font.h"


struct area_t {
  uint16_t begin, end;
  std::function<void(uint8_t button)> action;
};

struct padding_t {
  uint8_t size;
  bool pad_ends;
};

enum font_color_e { NORMAL_COLOR, ACCENT_COLOR };

struct text_segment_t {
  std::string str;
  font_color_e color;

  // TODO
  /* typename Fonts::Font* font; */
  /* typename DS::font_color font_color; */
};

struct segment_t {
  std::vector<text_segment_t> segments;
  std::optional<std::function<void(uint8_t)>> action;

  /* padding_t padding; */
};
