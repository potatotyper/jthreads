#pragma once

#include <atomic>

class Spinlock {
  std::atomic_flag flag = ATOMIC_FLAG_INIT;
public:
  void lock() noexcept { while (flag.test_and_set(std::memory_order_acquire)) { /* spin */; } }
  void unlock() noexcept { flag.clear(std::memory_order_release); }
};
