#pragma once

#include <array>
#include <tuple>

constexpr bool TOPBAR = true;
constexpr bool FORCE_DOCK = false;
constexpr int BAR_WIDTH = 5760, BAR_HEIGHT = 20, BAR_X_OFFSET = 0, BAR_Y_OFFSET = 0;
constexpr const char* WM_NAME = nullptr;
constexpr std::string_view WM_CLASS = "limebar";
constexpr int UNDERLINE_HEIGHT = 1;

// font name, y offset
constexpr std::array<std::tuple<const char*, int>, 1> FONTS = {
  std::make_tuple("GohuFont:pixelsize=11", 0) };
