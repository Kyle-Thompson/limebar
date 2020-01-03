#include "clock.h"

#include <chrono>
#include <ctime>
#include <thread>

constexpr static std::array<const char*, 12> months {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", };

void mod_clock::extract(ModulePixmap* px) const {
  px->write(current_time.data(), true);
  px->write(current_day.data());
}

void mod_clock::trigger() {
  std::this_thread::sleep_for(std::chrono::minutes(1));
}

void mod_clock::update() {
  time_t t = time(nullptr);
  struct tm* local = localtime(&t);

  snprintf(current_time.data(), 6, "%02d:%02d", local->tm_hour, local->tm_min);
  current_time[5] = '\0';

  snprintf(current_day.data(), 8, " %s %02d", months.at(local->tm_mon), local->tm_mday);
  current_day[7] = '\0';
}
