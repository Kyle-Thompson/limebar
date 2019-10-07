#include "fonts.h"

font_t::font_t(const char* pattern, int offset) {
  if ((xft_ft = X::Instance()->xft_font_open_name(pattern))) {
    descent = xft_ft->descent;
    height = xft_ft->ascent + descent;
    this->offset = offset;
  } else {
    fprintf(stderr, "Could not load font %s\n", pattern);
    exit(EXIT_FAILURE);
  }
}

void
Fonts::init() {
  // init fonts
  std::transform(FONTS.begin(), FONTS.end(), _fonts.begin(),
      [&](const auto& f) -> font_t {
        const auto& [font, offset] = f;
        return { font, offset };
      });

  // To make the alignment uniform, find maximum height
  const int maxh = std::max_element(_fonts.begin(), _fonts.end(),
      [](const auto& l, const auto& r){
        return l.height < r.height;
      })->height;

  // Set maximum height to all fonts
  for (auto& font : _fonts)
    font.height = maxh;
}

font_t&
Fonts::drawable_font(const uint16_t c)
{
  // If the end is reached without finding an appropriate font, return nullptr.
  // If the font can draw the character, return it.
  for (auto& font : _fonts) {
    if (font.font_has_glyph(c)) {
      return font;
    }
  }
  return _fonts[0];  // TODO: print error and exit?
}
