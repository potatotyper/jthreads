#include "jthreads/jthread.h"

#include "jthreads/tracer.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

struct jthread_t {
  uint64_t id = 0;
  std::string name;
  std::thread native;
  std::mutex mtx;
  std::condition_variable block_cv;
  std::condition_variable join_cv;
  bool finished = false;
  bool blocked = false;
  bool detached = false;
  bool join_started = false;
  bool join_done = false;
  bool has_native = false;
  void* return_value = nullptr;
};

struct jthread_mutex_t {
  uint64_t id = 0;
  std::string name;
  std::mutex native;
  std::mutex meta_mtx;
  jthread_t* owner = nullptr;
};

struct jthread_cond_t {
  uint64_t id = 0;
  std::string name;
  std::condition_variable cv;
  std::mutex meta_mtx;
  jthread_mutex_t* bound_mutex = nullptr;
  uint64_t waiters = 0;
};

namespace {

struct Registry {
  std::mutex mtx;
  std::vector<std::unique_ptr<jthread_t>> threads;
  std::atomic<uint64_t> next_thread_id{1};
  std::atomic<uint64_t> next_mutex_id{1};
  std::atomic<uint64_t> next_cond_id{1};
  bool initialized = false;
};

struct StartGate {
  std::mutex mtx;
  std::condition_variable cv;
  bool released = false;
};

thread_local jthread_t* current_thread = nullptr;

Registry& registry() {
  static Registry* instance = new Registry();
  return *instance;
}

void trace_event(
  const char* event,
  const char* state = "n/a",
  int64_t task_id = -1,
  int64_t duration_us = -1,
  int64_t queue_size = -1,
  const char* lock_name = nullptr,
  const char* flag_name = nullptr,
  int64_t flag_value = -1
) {
  Tracer::emit_event(event, state, task_id, duration_us, queue_size, lock_name, flag_name, flag_value);
}

std::string numbered_name(const char* prefix, uint64_t id) {
  return std::string(prefix) + "_" + std::to_string(id);
}

jthread_t* allocate_thread_descriptor(bool has_native, const char* state) {
  auto& reg = registry();
  auto thread = std::make_unique<jthread_t>();
  thread->id = reg.next_thread_id.fetch_add(1, std::memory_order_relaxed);
  thread->name = numbered_name("jthread", thread->id);
  thread->has_native = has_native;

  jthread_t* raw = thread.get();
  {
    std::lock_guard<std::mutex> lk(reg.mtx);
    reg.threads.push_back(std::move(thread));
  }

  trace_event("jthread_register", state, static_cast<int64_t>(raw->id));
  return raw;
}

jthread_t* register_current_thread(const char* state) {
  if (current_thread != nullptr) {
    return current_thread;
  }

  current_thread = allocate_thread_descriptor(false, state);
  return current_thread;
}

void ensure_initialized() {
  bool should_init = false;
  {
    auto& reg = registry();
    std::lock_guard<std::mutex> lk(reg.mtx);
    if (!reg.initialized) {
      reg.initialized = true;
      should_init = true;
    }
  }

  if (should_init) {
    Tracer::init("trace1.json");
    std::atexit([] { Tracer::shutdown(); });
    register_current_thread("main_thread");
    trace_event("jthread_init", "initialized");
    trace_event("flag_set", "jthreads_running", -1, -1, -1, nullptr, "jthreads", 1);
  } else {
    register_current_thread("external_thread");
  }
}

void require_thread(jthread_t* thread, const char* function_name) {
  if (thread == nullptr) {
    trace_event(function_name, "null_thread");
    throw std::invalid_argument(std::string(function_name) + " received a null jthread_t");
  }
}

void require_mutex(jthread_mutex_t* mutex, const char* function_name) {
  if (mutex == nullptr) {
    trace_event(function_name, "null_mutex");
    throw std::invalid_argument(std::string(function_name) + " received a null jthread_mutex_t");
  }
}

void require_cond(jthread_cond_t* cond, const char* function_name) {
  if (cond == nullptr) {
    trace_event(function_name, "null_cond");
    throw std::invalid_argument(std::string(function_name) + " received a null jthread_cond_t");
  }
}

}  // namespace

void jthread_init(const std::string& trace_path) {
  bool should_init = false;
  {
    auto& reg = registry();
    std::lock_guard<std::mutex> lk(reg.mtx);
    if (!reg.initialized) {
      reg.initialized = true;
      should_init = true;
    }
  }

  if (!should_init) {
    register_current_thread("external_thread");
    trace_event("jthread_init", "already_initialized");
    return;
  }

  Tracer::init(trace_path);
  std::atexit([] { Tracer::shutdown(); });
  register_current_thread("main_thread");
  trace_event("jthread_init", "initialized");
  trace_event("flag_set", "jthreads_running", -1, -1, -1, nullptr, "jthreads", 1);
}

jthread_t* jthread_create(jthread_start_t start_proc, void* start_arg) {
  ensure_initialized();
  if (start_proc == nullptr) {
    trace_event("jthread_create", "null_start_proc");
    throw std::invalid_argument("jthread_create received a null start procedure");
  }

  jthread_t* thread = allocate_thread_descriptor(true, "created");
  trace_event("jthread_create", "created", static_cast<int64_t>(thread->id));

  try {
    auto start_gate = std::make_shared<StartGate>();
    thread->native = std::thread([thread, start_proc, start_arg, start_gate] {
      {
        std::unique_lock<std::mutex> lk(start_gate->mtx);
        start_gate->cv.wait(lk, [start_gate] { return start_gate->released; });
      }

      current_thread = thread;
      trace_event("jthread_start", "running", static_cast<int64_t>(thread->id));

      void* result = nullptr;
      const auto run_begin = std::chrono::steady_clock::now();
      const char* finish_state = "finished";
      try {
        result = start_proc(start_arg);
      } catch (...) {
        finish_state = "exception";
      }

      const auto run_end = std::chrono::steady_clock::now();
      const auto run_us = std::chrono::duration_cast<std::chrono::microseconds>(run_end - run_begin).count();
      {
        std::lock_guard<std::mutex> lk(thread->mtx);
        thread->return_value = result;
        thread->finished = true;
        thread->blocked = false;
      }
      thread->block_cv.notify_all();
      thread->join_cv.notify_all();
      trace_event("jthread_finish", finish_state, static_cast<int64_t>(thread->id), run_us);
    });

    {
      std::lock_guard<std::mutex> lk(start_gate->mtx);
      start_gate->released = true;
    }
    start_gate->cv.notify_one();
  } catch (...) {
    {
      std::lock_guard<std::mutex> lk(thread->mtx);
      thread->finished = true;
    }
    trace_event("jthread_create", "native_thread_failed", static_cast<int64_t>(thread->id));
    throw;
  }

  return thread;
}

jthread_t* jthread_self() {
  ensure_initialized();
  trace_event("jthread_self", "current", current_thread != nullptr ? static_cast<int64_t>(current_thread->id) : -1);
  return current_thread;
}

void jthread_block() {
  ensure_initialized();
  jthread_t* self = current_thread;
  trace_event("jthread_block_begin", "blocked", static_cast<int64_t>(self->id));

  {
    std::unique_lock<std::mutex> lk(self->mtx);
    self->blocked = true;
    self->block_cv.wait(lk, [self] { return !self->blocked; });
  }

  trace_event("jthread_block_end", "running", static_cast<int64_t>(self->id));
}

void jthread_unblock(jthread_t* thread) {
  ensure_initialized();
  require_thread(thread, "jthread_unblock");

  {
    std::lock_guard<std::mutex> lk(thread->mtx);
    thread->blocked = false;
  }
  thread->block_cv.notify_all();
  trace_event("jthread_unblock", "runnable", static_cast<int64_t>(thread->id));
}

void jthread_join(jthread_t* thread, void** value_ptr) {
  ensure_initialized();
  require_thread(thread, "jthread_join");

  jthread_t* self = current_thread;
  if (thread == self) {
    trace_event("jthread_join", "self_join_rejected", static_cast<int64_t>(thread->id));
    throw std::invalid_argument("jthread_join cannot join the current thread");
  }

  {
    std::unique_lock<std::mutex> lk(thread->mtx);
    if (!thread->has_native) {
      trace_event("jthread_join", "non_joinable_thread", static_cast<int64_t>(thread->id));
      throw std::invalid_argument("jthread_join target is not a jthread-created thread");
    }
    if (thread->detached) {
      trace_event("jthread_join", "detached_thread", static_cast<int64_t>(thread->id));
      throw std::invalid_argument("jthread_join target has been detached");
    }
    if (thread->join_started) {
      trace_event("jthread_join", "already_joined", static_cast<int64_t>(thread->id));
      throw std::invalid_argument("jthread_join target is already being joined or was joined");
    }

    thread->join_started = true;
    trace_event("jthread_join_wait", "joining", static_cast<int64_t>(thread->id));
    thread->join_cv.wait(lk, [thread] { return thread->finished; });
    if (value_ptr != nullptr) {
      *value_ptr = thread->return_value;
    }
  }

  if (thread->native.joinable()) {
    thread->native.join();
  }

  {
    std::lock_guard<std::mutex> lk(thread->mtx);
    thread->join_done = true;
  }
  trace_event("jthread_join_done", "joined", static_cast<int64_t>(thread->id));
}

void jthread_detach(jthread_t* thread) {
  ensure_initialized();
  require_thread(thread, "jthread_detach");

  {
    std::lock_guard<std::mutex> lk(thread->mtx);
    if (!thread->has_native) {
      trace_event("jthread_detach", "non_detachable_thread", static_cast<int64_t>(thread->id));
      throw std::invalid_argument("jthread_detach target is not a jthread-created thread");
    }
    if (thread->join_started) {
      trace_event("jthread_detach", "join_started", static_cast<int64_t>(thread->id));
      throw std::invalid_argument("jthread_detach target is already being joined or was joined");
    }
    if (thread->detached) {
      trace_event("jthread_detach", "already_detached", static_cast<int64_t>(thread->id));
      return;
    }
    thread->detached = true;
  }

  if (thread->native.joinable()) {
    thread->native.detach();
  }
  trace_event("jthread_detach", "detached", static_cast<int64_t>(thread->id));
}

void jthread_yield() {
  ensure_initialized();
  trace_event("jthread_yield", "yielding", static_cast<int64_t>(current_thread->id));
  std::this_thread::yield();
  trace_event("jthread_yield_done", "running", static_cast<int64_t>(current_thread->id));
}

jthread_mutex_t* jthread_mutex_create(const char* debug_name) {
  ensure_initialized();

  auto mutex = std::make_unique<jthread_mutex_t>();
  mutex->id = registry().next_mutex_id.fetch_add(1, std::memory_order_relaxed);
  mutex->name = debug_name != nullptr ? debug_name : numbered_name("jthread_mutex", mutex->id);
  jthread_mutex_t* raw = mutex.release();
  trace_event("jthread_mutex_create", "created", -1, -1, -1, raw->name.c_str());
  return raw;
}

void jthread_mutex_lock(jthread_mutex_t* mutex) {
  ensure_initialized();
  require_mutex(mutex, "jthread_mutex_lock");

  trace_event("lock_attempt", "waiting", -1, -1, -1, mutex->name.c_str());
  const auto lock_begin = std::chrono::steady_clock::now();
  mutex->native.lock();
  const auto lock_end = std::chrono::steady_clock::now();
  const auto wait_us = std::chrono::duration_cast<std::chrono::microseconds>(lock_end - lock_begin).count();

  {
    std::lock_guard<std::mutex> lk(mutex->meta_mtx);
    mutex->owner = current_thread;
  }
  trace_event("lock_acquired", "held", -1, wait_us, -1, mutex->name.c_str());
}

void jthread_mutex_unlock(jthread_mutex_t* mutex) {
  ensure_initialized();
  require_mutex(mutex, "jthread_mutex_unlock");

  {
    std::lock_guard<std::mutex> lk(mutex->meta_mtx);
    if (mutex->owner != current_thread) {
      trace_event("jthread_mutex_unlock", "not_owner", -1, -1, -1, mutex->name.c_str());
      throw std::runtime_error("jthread_mutex_unlock called by a thread that does not own the mutex");
    }
    mutex->owner = nullptr;
  }
  trace_event("lock_released", "released", -1, -1, -1, mutex->name.c_str());
  mutex->native.unlock();
}

void jthread_mutex_destroy(jthread_mutex_t* mutex) {
  ensure_initialized();
  require_mutex(mutex, "jthread_mutex_destroy");

  {
    std::lock_guard<std::mutex> lk(mutex->meta_mtx);
    if (mutex->owner != nullptr) {
      trace_event("jthread_mutex_destroy", "still_locked", -1, -1, -1, mutex->name.c_str());
      throw std::runtime_error("jthread_mutex_destroy called while mutex is locked");
    }
  }

  if (!mutex->native.try_lock()) {
    trace_event("jthread_mutex_destroy", "busy", -1, -1, -1, mutex->name.c_str());
    throw std::runtime_error("jthread_mutex_destroy called while mutex is busy");
  }
  mutex->native.unlock();

  const std::string name = mutex->name;
  delete mutex;
  trace_event("jthread_mutex_destroy", "destroyed", -1, -1, -1, name.c_str());
}

jthread_cond_t* jthread_cond_create(jthread_mutex_t* mutex, const char* debug_name) {
  ensure_initialized();

  auto cond = std::make_unique<jthread_cond_t>();
  cond->id = registry().next_cond_id.fetch_add(1, std::memory_order_relaxed);
  cond->name = debug_name != nullptr ? debug_name : numbered_name("jthread_cond", cond->id);
  cond->bound_mutex = mutex;
  jthread_cond_t* raw = cond.release();
  trace_event("jthread_cond_create", "created", -1, -1, -1, raw->name.c_str());
  return raw;
}

void jthread_cond_wait(jthread_cond_t* cond) {
  ensure_initialized();
  require_cond(cond, "jthread_cond_wait");

  jthread_mutex_t* mutex = nullptr;
  {
    std::lock_guard<std::mutex> lk(cond->meta_mtx);
    mutex = cond->bound_mutex;
  }
  if (mutex == nullptr) {
    trace_event("jthread_cond_wait", "missing_mutex", -1, -1, -1, cond->name.c_str());
    throw std::invalid_argument("jthread_cond_wait requires a bound mutex or an explicit mutex");
  }

  jthread_cond_wait(cond, mutex);
}

void jthread_cond_wait(jthread_cond_t* cond, jthread_mutex_t* mutex) {
  ensure_initialized();
  require_cond(cond, "jthread_cond_wait");
  require_mutex(mutex, "jthread_cond_wait");

  trace_event("jthread_cond_wait_begin", "blocked_on_condition", -1, -1, -1, cond->name.c_str());
  {
    std::lock_guard<std::mutex> lk(cond->meta_mtx);
    if (cond->bound_mutex != nullptr && cond->bound_mutex != mutex) {
      trace_event("jthread_cond_wait", "wrong_mutex", -1, -1, -1, cond->name.c_str());
      throw std::invalid_argument("jthread_cond_wait called with a different mutex than the condition was bound to");
    }
    ++cond->waiters;
    if (cond->bound_mutex == nullptr) {
      cond->bound_mutex = mutex;
    }
  }

  {
    std::lock_guard<std::mutex> lk(mutex->meta_mtx);
    if (mutex->owner != current_thread) {
      trace_event("jthread_cond_wait", "mutex_not_owned", -1, -1, -1, mutex->name.c_str());
      {
        std::lock_guard<std::mutex> cond_lk(cond->meta_mtx);
        --cond->waiters;
      }
      throw std::runtime_error("jthread_cond_wait requires the current thread to own the mutex");
    }
    mutex->owner = nullptr;
  }
  trace_event("lock_released", "cond_wait_release", -1, -1, -1, mutex->name.c_str());

  std::unique_lock<std::mutex> lk(mutex->native, std::adopt_lock);
  const auto wait_begin = std::chrono::steady_clock::now();
  cond->cv.wait(lk);
  const auto wait_end = std::chrono::steady_clock::now();
  const auto wait_us = std::chrono::duration_cast<std::chrono::microseconds>(wait_end - wait_begin).count();
  lk.release();

  {
    std::lock_guard<std::mutex> meta_lk(mutex->meta_mtx);
    mutex->owner = current_thread;
  }
  {
    std::lock_guard<std::mutex> cond_lk(cond->meta_mtx);
    --cond->waiters;
  }

  trace_event("jthread_cond_wait_end", "woken", -1, wait_us, -1, cond->name.c_str());
  trace_event("lock_acquired", "cond_wait_reacquired", -1, wait_us, -1, mutex->name.c_str());
}

void jthread_cond_signal(jthread_cond_t* cond) {
  ensure_initialized();
  require_cond(cond, "jthread_cond_signal");

  cond->cv.notify_one();
  trace_event("jthread_cond_signal", "waking_one", -1, -1, -1, cond->name.c_str());
}

void jthread_cond_broadcast(jthread_cond_t* cond) {
  ensure_initialized();
  require_cond(cond, "jthread_cond_broadcast");

  cond->cv.notify_all();
  trace_event("jthread_cond_broadcast", "waking_all", -1, -1, -1, cond->name.c_str());
}

void jthread_cond_destroy(jthread_cond_t* cond) {
  ensure_initialized();
  require_cond(cond, "jthread_cond_destroy");

  {
    std::lock_guard<std::mutex> lk(cond->meta_mtx);
    if (cond->waiters != 0) {
      trace_event("jthread_cond_destroy", "waiters_present", -1, static_cast<int64_t>(cond->waiters), -1, cond->name.c_str());
      throw std::runtime_error("jthread_cond_destroy called while threads are waiting");
    }
  }

  const std::string name = cond->name;
  delete cond;
  trace_event("jthread_cond_destroy", "destroyed", -1, -1, -1, name.c_str());
}
