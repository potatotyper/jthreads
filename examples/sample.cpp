#include "jthreads/jthread.h"
#include "jthreads/tracer.h"

#include <array>
#include <chrono>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>

namespace {

constexpr int kWorkerCount = 5;

struct DemoState {
  jthread_mutex_t* counter_mutex = nullptr;
  jthread_mutex_t* word_mutex = nullptr;
  jthread_cond_t* counter_cv = nullptr;
  jthread_cond_t* word_cv = nullptr;

  int counter = 0;
  int word_changes = 0;
  std::string word = "initial";
};

void sleep_random_seconds(int worker_id) {
  const int seconds = (std::rand() % 3) + 3;
  jthread_sleep(worker_id, seconds);
}

void change_counter(DemoState* state, int worker_id) {
  jthread_mutex_lock(state->counter_mutex);
  Tracer::emit_event("sample_counter_change_begin", "running", worker_id);
  std::this_thread::sleep_for(std::chrono::seconds(1));

  ++state->counter;
  Tracer::emit_event("sample_counter_changed", "signaling_counter", worker_id);
  jthread_cond_signal(state->counter_cv);
  jthread_mutex_unlock(state->counter_mutex);

  jthread_yield();
}

void change_string(DemoState* state, int worker_id, const char* next_word) {
  jthread_mutex_lock(state->word_mutex);
  Tracer::emit_event("sample_string_change_begin", "running", worker_id);
  std::this_thread::sleep_for(std::chrono::seconds(1));

  state->word = next_word;
  ++state->word_changes;
  Tracer::emit_event("sample_word_changed", "signaling_word", worker_id);
  jthread_cond_signal(state->word_cv);
  jthread_mutex_unlock(state->word_mutex);

  jthread_yield();
}

void* thread_one(void* arg) {
  auto* state = static_cast<DemoState*>(arg);
  (void)jthread_self();

  sleep_random_seconds(1);
  change_counter(state, 1);
  sleep_random_seconds(1);
  change_string(state, 1, "apple");
  return nullptr;
}

void* thread_two(void* arg) {
  auto* state = static_cast<DemoState*>(arg);
  (void)jthread_self();

  sleep_random_seconds(2);
  change_string(state, 2, "bravo");
  sleep_random_seconds(2);
  change_counter(state, 2);
  return nullptr;
}

void* thread_three(void* arg) {
  auto* state = static_cast<DemoState*>(arg);
  (void)jthread_self();

  sleep_random_seconds(3);
  change_counter(state, 3);
  sleep_random_seconds(3);
  change_string(state, 3, "charlie");
  return nullptr;
}

void* thread_four(void* arg) {
  auto* state = static_cast<DemoState*>(arg);
  (void)jthread_self();

  sleep_random_seconds(4);
  change_string(state, 4, "delta");
  sleep_random_seconds(4);
  change_counter(state, 4);
  return nullptr;
}

void* thread_five(void* arg) {
  auto* state = static_cast<DemoState*>(arg);
  (void)jthread_self();

  sleep_random_seconds(5);
  change_counter(state, 5);
  sleep_random_seconds(5);
  change_string(state, 5, "echo");
  return nullptr;
}

void destroy_sync_primitives(DemoState& state) {
  jthread_cond_destroy(state.word_cv);
  jthread_cond_destroy(state.counter_cv);
  jthread_mutex_destroy(state.word_mutex);
  jthread_mutex_destroy(state.counter_mutex);
}

}  // namespace

int main() {
  jthread_init("trace1.json");
  Tracer::emit_event("sample_begin", "five_simple_worker_functions");

  DemoState state;
  state.counter_mutex = jthread_mutex_create("counter_mutex");
  state.word_mutex = jthread_mutex_create("word_mutex");
  state.counter_cv = jthread_cond_create(state.counter_mutex, "counter_cv");
  state.word_cv = jthread_cond_create(state.word_mutex, "word_cv");

  std::array<jthread_t*, kWorkerCount> workers = {
    jthread_create(thread_one, &state),
    jthread_create(thread_two, &state),
    jthread_create(thread_three, &state),
    jthread_create(thread_four, &state),
    jthread_create(thread_five, &state),
  };

  for (jthread_t* worker : workers) {
    jthread_join(worker);
  }

  const int final_counter = state.counter;
  const int final_word_changes = state.word_changes;
  const std::string final_word = state.word;
  destroy_sync_primitives(state);

  std::cout << "counter: " << final_counter << "\n";
  std::cout << "word changes: " << final_word_changes << "\n";
  std::cout << "final word: " << final_word << "\n";
  std::cout << "trace written to trace1.json\n";

  return 0;
}
