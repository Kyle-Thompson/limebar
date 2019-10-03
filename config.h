#pragma once

#include <array>
#include <tuple>
#include <unordered_map>

#include "modules/module.h"
#include "modules/workspaces.h"
#include "modules/windows.h"
#include "modules/clock.h"

constexpr bool TOPBAR = true;
constexpr bool FORCE_DOCK = false;
constexpr int BAR_WIDTH = 5760, BAR_HEIGHT = 20, BAR_X_OFFSET = 0, BAR_Y_OFFSET = 0;
constexpr const char* WM_NAME = nullptr;
constexpr std::string_view WM_CLASS = "limebar";
constexpr int UNDERLINE_HEIGHT = 1;

// font name, y offset
constexpr std::array<std::tuple<const char*, int>, 1> FONTS = {
  std::make_tuple("GohuFont:pixelsize=11", 0) };

const auto modules = [] {
  std::unordered_map<const char*, std::unique_ptr<Module>> _modules;
  _modules.emplace("workspaces", std::make_unique<mod_workspaces>());
  _modules.emplace("windows", std::make_unique<mod_windows>());
  _modules.emplace("clock", std::make_unique<mod_clock>());
  return _modules;
}();
