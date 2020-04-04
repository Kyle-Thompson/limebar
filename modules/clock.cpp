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
  co_yield {.segments{{.str = current_time.data(), .color = ACCENT_COLOR},
                      {.str = current_date.data(), .color = NORMAL_COLOR}}};
}

void
mod_clock::trigger() {
  std::this_thread::sleep_for(std::chrono::minutes(1));
}

void
mod_clock::update() {
  time_t t = time(nullptr);
  struct tm* local = localtime(&t);

  snprintf(current_time.data(), 6, "%02d:%02d", local->tm_hour, local->tm_min);
  current_time[5] = '\0';

  snprintf(current_date.data(), 8, " %s %02d", months.at(local->tm_mon),
           local->tm_mday);
  current_date[7] = '\0';
}
