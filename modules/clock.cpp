#include "clock.h"

#include <chrono>
#include <ctime>
#include <thread>

constexpr static std::array<const char*, 12> months {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", };

void mod_clock::get(ModulePixmap& px) const {
  px.write_with_accent(current_time);
  px.write(current_day);
}

void mod_clock::trigger()
{
  std::this_thread::sleep_for(std::chrono::minutes(1));
}

void mod_clock::update()
{
  time_t t = time(nullptr);
  struct tm* local = localtime(&t);

  snprintf(current_time, 6, "%02d:%02d", local->tm_hour, local->tm_min);
  current_time[5] = '\0';

  snprintf(current_day, 8, " %s %02d", months[local->tm_mon], local->tm_mday);
  current_day[7] = '\0';
}
