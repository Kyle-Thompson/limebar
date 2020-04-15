#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>


using ucs2 = std::vector<uint16_t>;


struct coordinate_t {
  int16_t x;
  int16_t y;
};

/** rectangle_t
 * Utility struct for representing a rectangle.
 * NOTE: {x,y} refers to the coordinate pair for the top left pixel.
 */
struct rectangle_t {
  int16_t x{0};
  int16_t y{0};
  uint16_t width{0};
  uint16_t height{0};
};

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
  /* typename DS::font_t* font; */
  /* typename DS::font_color font_color; */
};

struct segment_t {
  std::vector<text_segment_t> segments;
  std::optional<std::function<void(uint8_t)>> action;

  /* padding_t padding; */
};
