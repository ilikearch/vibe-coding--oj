#include "log.h"
#include <iostream>
#include <ctime>
#include <mutex>
#include <chrono>
#include <iomanip>

static std::mutex log_mutex;
static LogLevel min_level = LOG_DEBUG;

static const char* level_str(LogLevel lv) {
    switch (lv) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO ";
        case LOG_WARN:  return "WARN ";
        case LOG_ERROR: return "ERROR";
    }
    return "????";
}

void log(LogLevel level, const std::string& msg) {
    if (level < min_level) return;
    std::lock_guard<std::mutex> lock(log_mutex);
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::tm local;
    localtime_r(&t, &local);
    std::cerr << std::put_time(&local, "%Y-%m-%d %H:%M:%S")
              << '.' << std::setfill('0') << std::setw(3) << ms.count()
              << " [" << level_str(level) << "] " << msg << std::endl;
}
