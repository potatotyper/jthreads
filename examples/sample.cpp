#include "jthreads/jthread.h"

#include <iostream>

namespace {

struct DemoState {
  jthread_mutex_t* mutex = nullptr;
  jthread_cond_t* ready_cv = nullptr;
  jthread_cond_t* start_cv = nullptr;
  int ready_count = 0;
  bool start = false;
  int counter = 0;
};

void* worker_main(void* arg) {
  auto* state = static_cast<DemoState*>(arg);

  jthread_mutex_lock(state->mutex);
  ++state->ready_count;
  jthread_cond_signal(state->ready_cv);

  while (!state->start) {
    jthread_cond_wait(state->start_cv);
  }

  auto* result = new int(++state->counter);
  jthread_mutex_unlock(state->mutex);
  return result;
}

}  // namespace

int main() {
  jthread_init("trace1.json");

  DemoState state;
  state.mutex = jthread_mutex_create("demo_mutex");
  state.ready_cv = jthread_cond_create(state.mutex, "ready_cv");
  state.start_cv = jthread_cond_create(state.mutex, "start_cv");

  jthread_t* first = jthread_create(worker_main, &state);
  jthread_t* second = jthread_create(worker_main, &state);

  jthread_mutex_lock(state.mutex);
  while (state.ready_count < 2) {
    jthread_cond_wait(state.ready_cv);
  }
  state.start = true;
  jthread_cond_broadcast(state.start_cv);
  jthread_mutex_unlock(state.mutex);

  void* first_value = nullptr;
  void* second_value = nullptr;
  jthread_join(first, &first_value);
  jthread_join(second, &second_value);

  auto* first_result = static_cast<int*>(first_value);
  auto* second_result = static_cast<int*>(second_value);
  std::cout << "worker results: " << *first_result << ", " << *second_result << "\n";

  delete first_result;
  delete second_result;
  jthread_cond_destroy(state.start_cv);
  jthread_cond_destroy(state.ready_cv);
  jthread_mutex_destroy(state.mutex);

  return 0;
}
