#include "jthreads/tracer.h"
#include <iostream>
#include <chrono>
#include <unordered_map>
#include <thread>
#include <iomanip>

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

std::string json_escape(const char* raw) {
  std::ostringstream escaped;
  const std::string value = raw != nullptr ? raw : "";
  for (const char ch : value) {
    switch (ch) {
      case '"':
        escaped << "\\\"";
        break;
      case '\\':
        escaped << "\\\\";
        break;
      case '\b':
        escaped << "\\b";
        break;
      case '\f':
        escaped << "\\f";
        break;
      case '\n':
        escaped << "\\n";
        break;
      case '\r':
        escaped << "\\r";
        break;
      case '\t':
        escaped << "\\t";
        break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          escaped << "\\u"
                  << std::hex << std::setw(4) << std::setfill('0')
                  << static_cast<int>(static_cast<unsigned char>(ch))
                  << std::dec << std::setfill(' ');
        } else {
          escaped << ch;
        }
        break;
    }
  }
  return escaped.str();
}
}

std::mutex Tracer::mtx;
std::ofstream Tracer::out;

void Tracer::init(const std::string& path) {
  std::lock_guard<std::mutex> g(mtx);
  if (out.is_open()) {
    out.close();
  }
  out.open(path, std::ios::out | std::ios::trunc);
}

void Tracer::emit(const std::string& json_line) {
  std::lock_guard<std::mutex> g(mtx);
  if (out.is_open()) {
    out << json_line << "\n";
  }
}

void Tracer::emit_event(
  const char* event,
  const char* state,
  int64_t task_id,
  int64_t duration_us,
  int64_t queue_size,
  const char* lock_name,
  const char* flag_name,
  int64_t flag_value
) {
  std::ostringstream oss;
  oss << "{\"event\":\"" << json_escape(event) << "\"";
  oss << ",\"state\":\"" << json_escape(state != nullptr ? state : "n/a") << "\"";

  if (task_id >= 0) {
    oss << ",\"task_id\":" << task_id;
  }
  if (queue_size >= 0) {
    oss << ",\"queue_size\":" << queue_size;
  }
  if (duration_us >= 0) {
    oss << ",\"duration_us\":" << duration_us;
  }
  if (lock_name != nullptr) {
    oss << ",\"lock\":\"" << json_escape(lock_name) << "\"";
  }
  if (flag_name != nullptr) {
    oss << ",\"flag\":\"" << json_escape(flag_name) << "\"";
  }
  if (flag_value >= 0) {
    oss << ",\"flag_value\":" << flag_value;
  }

  oss << ",\"time_since_started_s\":" << std::fixed << std::setprecision(6) << relative_time_now_seconds();
  oss << ",\"thread\":" << thread_number();
  oss << '}';
  emit(oss.str());
}

void Tracer::shutdown() {
  std::lock_guard<std::mutex> g(mtx);
  if (out.is_open()) out.close();
}
