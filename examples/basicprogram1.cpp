#include "jthreads/threadpool.h"
#include "jthreads/tracer.h"
#include <iostream>
#include <chrono>

int main() {
  Tracer::init("trace1.json");
  FixedThreadPool pool(4);

  auto f1 = pool.submit([]{
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    std::cout << "task 1\n";
  });
  auto f2 = pool.submit([]{
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    std::cout << "task 2\n";
  });

  f1.get(); f2.get();
  pool.shutdown();
  return 0;
}
