#pragma once

#include <string>

enum LogLevel { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };

void log(LogLevel level, const std::string& msg);

#define LOG_DEBUG(msg) log(LOG_DEBUG, msg)
#define LOG_INFO(msg)  log(LOG_INFO,  msg)
#define LOG_WARN(msg)  log(LOG_WARN,  msg)
#define LOG_ERROR(msg) log(LOG_ERROR, msg)
