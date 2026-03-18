#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <sstream>

class Tracer {
public:
  static void init(const std::string& path = "trace.jsonl");
  static void emit(const std::string& json_line);
  static void shutdown();
private:
  static std::mutex mtx;
  static std::ofstream out;
};
