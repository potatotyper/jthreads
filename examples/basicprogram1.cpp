#include "jthreads/threadpool.h"
#include "jthreads/tracer.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

std::mutex g_stats_mutex;
std::vector<int64_t> g_results;

void traced_sleep(std::chrono::milliseconds duration, int task_id, const char* state) {
  Tracer::emit_event("task_sleep_begin", state, task_id, -1, -1, nullptr, nullptr, -1);
  std::this_thread::sleep_for(duration);
  Tracer::emit_event(
    "task_sleep_end",
    state,
    task_id,
    static_cast<int64_t>(duration.count() * 1000),
    -1,
    nullptr,
    nullptr,
    -1
  );
}

void record_result_with_contention(int task_id, int64_t value, std::chrono::milliseconds hold_time) {
  Tracer::emit_event("lock_attempt", "stats_waiting", task_id, -1, -1, "stats_mtx");
  auto wait_begin = std::chrono::steady_clock::now();

  std::unique_lock<std::mutex> lk(g_stats_mutex);

  auto wait_end = std::chrono::steady_clock::now();
  auto wait_us = std::chrono::duration_cast<std::chrono::microseconds>(wait_end - wait_begin).count();
  Tracer::emit_event("lock_acquired", "stats_held", task_id, wait_us, -1, "stats_mtx");

  g_results.push_back(value);
  traced_sleep(hold_time, task_id, "holding_stats_lock");

  Tracer::emit_event(
    "lock_released",
    "stats_released",
    task_id,
    -1,
    static_cast<int64_t>(g_results.size()),
    "stats_mtx"
  );
}

int64_t heavy_compute(int n) {
  int64_t acc = 0;
  for (int i = 1; i <= n; ++i) {
    acc += static_cast<int64_t>(i) * static_cast<int64_t>(i);
  }
  return acc;
}

}  // namespace

int main() {
  Tracer::init("trace1.json");
  Tracer::emit_event("flag_set", "demo_start", -1, -1, -1, nullptr, "demo_phase", 0);

  constexpr int kWorkers = 4;
  constexpr int kTasks = 16;

  FixedThreadPool pool(kWorkers);
  std::vector<std::future<int64_t>> futures;
  futures.reserve(kTasks);

  for (int i = 0; i < kTasks; ++i) {
    futures.push_back(pool.submit([i]() -> int64_t {
      Tracer::emit_event("task_print", "task_begin", i + 1);

      // Mix short and long sleeps so timelines clearly overlap.
      traced_sleep(std::chrono::milliseconds(10 + (i % 5) * 12), i + 1, "phase_sleep_a");

      const int64_t computed = heavy_compute(3000 + i * 450);

      // Alternate lock hold times to produce visible contention intervals.
      record_result_with_contention(i + 1, computed, std::chrono::milliseconds(8 + (i % 3) * 10));

      traced_sleep(std::chrono::milliseconds(5 + (i % 4) * 7), i + 1, "phase_sleep_b");
      Tracer::emit_event("task_print", "task_finish", i + 1);
      return computed;
    }));
  }

  Tracer::emit_event("flag_set", "collecting_results", -1, -1, -1, nullptr, "demo_phase", 1);

  int64_t total = 0;
  for (auto& f : futures) {
    total += f.get();
  }

  Tracer::emit_event("flag_set", "results_collected", -1, -1, -1, nullptr, "demo_phase", 2);

  {
    std::lock_guard<std::mutex> guard(g_stats_mutex);
    std::sort(g_results.begin(), g_results.end());
  }

  pool.shutdown();

  Tracer::emit_event("flag_set", "demo_complete", -1, -1, -1, nullptr, "demo_phase", 3);
  Tracer::emit_event("task_print", "summary", -1, -1, static_cast<int64_t>(g_results.size()));

  std::cout << "Completed " << kTasks << " tasks across " << kWorkers << " workers\n";
  std::cout << "Aggregate compute checksum: " << total << "\n";
  std::cout << "Recorded results: " << g_results.size() << "\n";
  std::cout << "Trace written to trace1.json\n";

  return 0;
}
