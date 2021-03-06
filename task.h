#pragma once

#include <tuple>
/* #include <concepts> */


template <typename T>
concept Taskable = requires(T t) {
  /* { t.has_work() } -> std::same_as<bool>; */
  /* { t.do_work() } -> std::same_as<void>; */
  t.has_work();
  t.do_work();
};

template <typename T>
concept Downstream = requires(T t) {
  t.update();
};


/** Task
 * A greedy task meant for asynchronous use which immediately runs any work it
 * has and updates the downstream when there is no more work to do on itself.
 */
template <Taskable T, Downstream... D>
class Task {
 public:
  explicit Task(T* t, D*... d) : _task(t), _downstream(d...) {}

  void work() {
    if (!has_work()) {
      return;
    }

    do {
      do_work();
    } while (has_work());

    update();
  }

 protected:
  bool has_work() { return _task->has_work(); }
  void do_work() { _task->do_work(); }
  void update() {
    std::apply([](D*... d) { ((d->update()), ...); }, _downstream);
  }

 private:
  T* _task;
  std::tuple<D*...> _downstream;
};


/** TaskModule
 * A specialization of Task that runs the task on construction. Useful for
 * modules to get their initial values.
 */
template <Taskable T, Downstream... D>
class ModuleTask : public Task<T, D...> {
 public:
  explicit ModuleTask(T* t, D*... d) : Task<T, D...>(t, d...) {
    Task<T, D...>::do_work();
  }
};
