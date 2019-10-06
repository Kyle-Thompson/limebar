#include "clock.h"

#include <chrono>
#include <ctime>
#include <thread>

constexpr static std::array<const char*, 12> months {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", };

void mod_clock::trigger()
{
  std::this_thread::sleep_for(std::chrono::minutes(1));
}

std::string mod_clock::update()
{
  time_t t = time(nullptr);
  struct tm* local = localtime(&t);
  snprintf(clock_str, 35, "%%{F#257fad}%02d:%02d%%{F#7ea2b4} %s %02d",
      local->tm_hour, local->tm_min, months[local->tm_mon], local->tm_mday);
  clock_str[34] = '\0';
  return clock_str;
}
