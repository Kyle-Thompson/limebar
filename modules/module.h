#pragma once

#include <condition_variable>
#include <mutex>

class module {
 public:
  module() {}
  virtual ~module() {}

  std::string get() {
    std::lock_guard<std::mutex> g(_mutex);
    return _str;
  }
  void operator()() {
    update();
    while (true) {
      trigger();
      update();
    }
  }

  static std::condition_variable condvar;

 protected:
  void set(std::string str) {
    std::lock_guard<std::mutex> g(_mutex);
    _str = str;
    condvar.notify_one();
  }
  virtual void trigger() = 0;
  virtual void update()  = 0;
  std::mutex _mutex;
  std::string _str;
};
