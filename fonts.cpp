#include "fonts.h"

font_t::font_t(const char* pattern, int offset, xcb_connection_t *c, int scr_nbr) {
  xcb_query_font_cookie_t queryreq;
  xcb_query_font_reply_t *font_info;
  xcb_void_cookie_t cookie;
  xcb_font_t font;

  font = xcb_generate_id(c);

  cookie = xcb_open_font_checked(c, font, strlen(pattern), pattern);
  if (!xcb_request_check (c, cookie)) {
    queryreq = xcb_query_font(c, font);
    font_info = xcb_query_font_reply(c, queryreq, nullptr);

    ptr = font;
    descent = font_info->font_descent;
    height = font_info->font_ascent + font_info->font_descent;
    width = font_info->max_bounds.character_width;
    char_max = font_info->max_byte1 << 8 | font_info->max_char_or_byte2;
    char_min = font_info->min_byte1 << 8 | font_info->min_char_or_byte2;
    this->offset = offset;
    // Copy over the width lut as it's part of font_info
    int lut_size = sizeof(xcb_charinfo_t) * xcb_query_font_char_infos_length(font_info);
    if (lut_size) {
      width_lut = (xcb_charinfo_t *) malloc(lut_size);
      memcpy(width_lut, xcb_query_font_char_infos(font_info), lut_size);
    }
    free(font_info);
  } else if ((xft_ft = DisplayManager::Instance()->xft_font_open_name(scr_nbr, pattern))) {
    ascent = xft_ft->ascent;
    descent = xft_ft->descent;
    height = ascent + descent;
    this->offset = offset;
  } else {
    fprintf(stderr, "Could not load font %s\n", pattern);
    exit(EXIT_FAILURE);
  }
}

bool
font_t::font_has_glyph (const uint16_t c)
{
  if (xft_ft)
    return DisplayManager::Instance()->xft_char_exists(xft_ft, (FcChar32) c);

  if (c < char_min || c > char_max)
    return false;

  if (width_lut && width_lut[c - char_min].character_width == 0)
    return false;

  return true;
}

void Fonts::init(xcb_connection_t *c, int scr_nbr) {
  // init fonts
  std::transform(FONTS.begin(), FONTS.end(), _fonts.begin(),
      [&](const auto& f) -> font_t {
        const auto& [font, offset] = f;
        return { font, offset, c, scr_nbr };
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

font_t *
Fonts::select_drawable_font(const uint16_t c)
{
  // If the user has specified a font to use, try that first.
  if (_index != -1 && current()->font_has_glyph(c)) {
    return current();
  }

  // If the end is reached without finding an appropriate font, return nullptr.
  // If the font can draw the character, return it.
  for (auto& font : _fonts) {
    if (font.font_has_glyph(c)) {
      return &font;
    }
  }
  return nullptr;
}
