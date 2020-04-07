#include "clock.h"

#include <chrono>
#include <ctime>
#include <thread>

#include "../types.h"

constexpr static std::array<const char*, 12> months{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

cppcoro::generator<segment_t>
mod_clock::extract() const {
  for (auto seg : _segments) {
    co_yield seg;
  }
}

void
mod_clock::trigger() {
  std::this_thread::sleep_for(std::chrono::minutes(1));
}

void
mod_clock::update() {
  // TODO: use chrono and formatting
  time_t t = time(nullptr);
  struct tm* local = localtime(&t);

  std::array<char, 6> current_time;
  snprintf(current_time.data(), current_time.size(), "%02d:%02d", local->tm_hour, local->tm_min);
  *current_time.rbegin() = '\0';

  std::array<char, 8> current_date;
  snprintf(current_date.data(), current_date.size(), " %s %02d", months.at(local->tm_mon),
           local->tm_mday);
  *current_date.rbegin() = '\0';

  _segments[0] = {.segments{{.str = current_time.data(), .color = ACCENT_COLOR},
                            {.str = current_date.data(), .color = NORMAL_COLOR}}};
}
