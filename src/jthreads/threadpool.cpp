#include "jthreads/threadpool.h"
#include "jthreads/tracer.h"
#include <utility>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <unordered_map>

namespace {
std::chrono::steady_clock::time_point program_start_time() {
  static const auto t0 = std::chrono::steady_clock::now();
  return t0;
}

double relative_time_now_seconds() {
  return std::chrono::duration_cast<std::chrono::duration<double>>(
    std::chrono::steady_clock::now() - program_start_time()
  ).count();
}

uint64_t thread_number() {
  static std::mutex map_mtx;
  static std::unordered_map<std::thread::id, uint64_t> ids;
  static uint64_t next_id = 1;

  std::lock_guard<std::mutex> lk(map_mtx);
  const auto it = ids.find(std::this_thread::get_id());
  if (it != ids.end()) {
    return it->second;
  }

  const uint64_t assigned = next_id++;
  ids.emplace(std::this_thread::get_id(), assigned);
  return assigned;
}
}

void FixedThreadPool::trace_event(
  const char* event,
  int64_t task_id,
  int64_t duration_us,
  int64_t queue_size,
  const char* state
) {
  std::ostringstream oss;
  oss << "{\"event\":\"" << event << "\"";
  oss << ",\"state\":\"" << (state != nullptr ? state : "n/a") << "\"";

  if (task_id >= 0) {
    oss << ",\"task_id\":" << task_id;
  }
  if (queue_size >= 0) {
    oss << ",\"queue_size\":" << queue_size;
  }
  if (duration_us >= 0) {
    oss << ",\"duration_us\":" << duration_us;
  }

  oss << ",\"time_since_started_s\":" << std::fixed << std::setprecision(6) << relative_time_now_seconds();
  oss << ",\"thread\":" << thread_number();

  oss << '}';
  Tracer::emit(oss.str());
}

FixedThreadPool::FixedThreadPool(size_t num_workers) {
  (void)num_workers;
  trace_event("pool_start", -1, -1, -1, "starting_workers");
  for (size_t i = 0; i < num_workers; ++i) {
    workers_.emplace_back([this]{ worker_loop(); });
  }
}

FixedThreadPool::~FixedThreadPool() {
  shutdown();
}

void FixedThreadPool::shutdown() {
  trace_event("shutdown_begin");
  bool expected = true;
  if (running_.compare_exchange_strong(expected, false)) {
    tasks_cv_.notify_all();
    trace_event("shutdown_notify_all", -1, -1, -1, "waking_workers");
    for (auto &t : workers_) {
      if (t.joinable()) {
        trace_event("shutdown_join_wait", -1, -1, -1, "joining_worker");
        t.join();
        trace_event("shutdown_join_done", -1, -1, -1, "worker_joined");
      }
    }
    trace_event("shutdown_complete");
    Tracer::shutdown();
  } else {
    trace_event("shutdown_skip", -1, -1, -1, "already_stopped");
  }
}

void FixedThreadPool::worker_loop() {
  trace_event("worker_start", -1, -1, -1, "running");
  while (running_) {
    std::pair<uint64_t, std::function<void()>> task_pair;

    const auto lock_begin = std::chrono::steady_clock::now();
    {
      std::unique_lock<std::mutex> lk(tasks_mtx_);
      const auto lock_end = std::chrono::steady_clock::now();
      const auto lock_wait_us = std::chrono::duration_cast<std::chrono::microseconds>(
        lock_end - lock_begin
      ).count();
      trace_event("worker_queue_lock_acquired", -1, lock_wait_us);

      trace_event("worker_wait_begin", -1, -1, static_cast<int64_t>(tasks_.size()), "blocked_on_condition");
      const auto wait_begin = std::chrono::steady_clock::now();

      tasks_cv_.wait(lk, [this]{ return !tasks_.empty() || !running_; });
      const auto wait_end = std::chrono::steady_clock::now();
      const auto wait_us = std::chrono::duration_cast<std::chrono::microseconds>(
        wait_end - wait_begin
      ).count();
      trace_event("worker_wait_end", -1, wait_us, static_cast<int64_t>(tasks_.size()), "woken");

      if (!running_ && tasks_.empty()) {
        trace_event("worker_stop", -1, -1, 0, "shutdown_no_work");
        return;
      }
      task_pair = std::move(tasks_.front());
      tasks_.pop();
      trace_event(
        "task_dequeued",
        static_cast<int64_t>(task_pair.first),
        -1,
        static_cast<int64_t>(tasks_.size()),
        "ready_to_run"
      );
    }

    uint64_t task_id = task_pair.first;
    trace_event("task_start", static_cast<int64_t>(task_id), -1, -1, "running");
    const auto run_begin = std::chrono::steady_clock::now();

    task_pair.second();

    const auto run_end = std::chrono::steady_clock::now();
    const auto run_us = std::chrono::duration_cast<std::chrono::microseconds>(
      run_end - run_begin
    ).count();
    trace_event("task_end", static_cast<int64_t>(task_id), run_us, -1, "finished");
  }

  trace_event("worker_stop", -1, -1, -1, "running_flag_false");
}

