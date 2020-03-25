#pragma once

#include <string_view>

#include "x.h"

constexpr bool FORCE_DOCK = false;
constexpr const char* WM_NAME = nullptr;
constexpr std::string_view WM_CLASS = "limebar";

// specify the display server to use. (currently only supports X)
using DS = X;
