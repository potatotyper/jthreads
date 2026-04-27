#pragma once

#include <cstdint>
#include <string>
#include <mutex>
#include <fstream>
#include <sstream>

class Tracer {
public:
  static void init(const std::string& path = "trace.jsonl");
  static void emit(const std::string& json_line);
  static void emit_event(
    const char* event,
    const char* state = "n/a",
    int64_t task_id = -1,
    int64_t duration_us = -1,
    int64_t queue_size = -1,
    const char* lock_name = nullptr,
    const char* flag_name = nullptr,
    int64_t flag_value = -1
  );
  static void shutdown();
private:
  static std::mutex mtx;
  static std::ofstream out;
};
