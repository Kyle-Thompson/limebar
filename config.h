#pragma once

#include <array>

constexpr bool FORCE_DOCK = false;
constexpr int BAR_HEIGHT = 20;
constexpr const char* WM_NAME = nullptr;
constexpr std::string_view WM_CLASS = "limebar";

// font name, y offset
constexpr std::array<std::pair<const char*, int>, 1> FONTS = {
  std::make_pair("GohuFont:pixelsize=11", 0) };
