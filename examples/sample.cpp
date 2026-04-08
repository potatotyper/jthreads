#include "jthreads/threadpool.h"
#include "jthreads/tracer.h"
#include <iostream>
#include <chrono>

int main() {
  Tracer::init("trace1.json");
  FixedThreadPool pool(4);

  auto f1 = pool.submit([]{
    Tracer::emit_event("task_sleep_begin", "sleeping", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    Tracer::emit_event("task_sleep_end", "awake", 1);
    Tracer::emit_event("task_print", "printing", 1);
    std::cout << "task 1\n";
  });
  auto f2 = pool.submit([]{
    Tracer::emit_event("task_sleep_begin", "sleeping", 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    Tracer::emit_event("task_sleep_end", "awake", 2);
    Tracer::emit_event("task_print", "printing", 2);
    std::cout << "task 2\n";
  });

  f1.get();
  f2.get();
  pool.shutdown();
  return 0;
}
