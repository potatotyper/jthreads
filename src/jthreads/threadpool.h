#pragma once

#include <vector>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <cstdint>
#include <chrono>

class FixedThreadPool {
public:
  explicit FixedThreadPool(size_t num_workers);
  ~FixedThreadPool();

  template<class F, class... Args>
  std::future<std::invoke_result_t<F, Args...>> submit(F&& f, Args&&... args) {
    using R = std::invoke_result_t<F, Args...>;

    const uint64_t task_id = next_task_id_.fetch_add(1, std::memory_order_relaxed);
    trace_event("task_submit", static_cast<int64_t>(task_id));

    auto task_pack = std::make_shared<std::packaged_task<R()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    std::future<R> result = task_pack->get_future();

    const auto lock_wait_begin = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lk(tasks_mtx_);
    const auto lock_wait_end = std::chrono::steady_clock::now();
    const auto lock_wait_us = std::chrono::duration_cast<std::chrono::microseconds>(
      lock_wait_end - lock_wait_begin
    ).count();
    trace_event("submit_lock_acquired", static_cast<int64_t>(task_id), lock_wait_us);

    if (!running_) {
      trace_event("task_submit_rejected", static_cast<int64_t>(task_id), -1, -1, "pool_stopped");
      throw std::runtime_error("submit() called on stopped FixedThreadPool");
    }

    tasks_.emplace(task_id, [task_pack]{ (*task_pack)(); });
    const auto queue_size = static_cast<int64_t>(tasks_.size());
    lk.unlock();

    trace_event("task_enqueued", static_cast<int64_t>(task_id), -1, queue_size, "queued");
    tasks_cv_.notify_one();
    trace_event("worker_notified", static_cast<int64_t>(task_id));

    return result;
  }

  void shutdown();

private:
  static void trace_event(
    const char* event,
    int64_t task_id = -1,
    int64_t duration_us = -1,
    int64_t queue_size = -1,
    const char* state = nullptr
  );

  void worker_loop();

  std::vector<std::thread> workers_;
  std::queue<std::pair<uint64_t, std::function<void()>>> tasks_;
  std::mutex tasks_mtx_;
  std::condition_variable tasks_cv_;
  std::atomic<bool> running_{true};
  std::atomic<uint64_t> next_task_id_{1};
};
