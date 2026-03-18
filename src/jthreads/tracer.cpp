#include "jthreads/tracer.h"
#include <iostream>
#include <chrono>

std::mutex Tracer::mtx;
std::ofstream Tracer::out;

void Tracer::init(const std::string& path) {
  std::lock_guard<std::mutex> g(mtx);
  out.open(path, std::ios::out | std::ios::trunc);
}

void Tracer::emit(const std::string& json_line) {
  std::lock_guard<std::mutex> g(mtx);
  if (out.is_open()) {
    out << json_line << "\n";
  }
}

void Tracer::shutdown() {
  std::lock_guard<std::mutex> g(mtx);
  if (out.is_open()) out.close();
}
