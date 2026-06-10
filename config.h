#pragma once

#include <string>

constexpr const char* DB_HOST = "localhost";
constexpr int DB_PORT = 0;
constexpr const char* DB_USER = "root";
constexpr const char* DB_PASS = "";
constexpr const char* DB_NAME = "vibe_oj";

constexpr int JUDGE_CPU_TIMEOUT_SEC = 1;
constexpr int JUDGE_REAL_TIMEOUT_MS = 2000;
constexpr int JUDGE_MEMORY_LIMIT_MB = 128;
constexpr int JUDGE_OUTPUT_LIMIT_MB = 1;
constexpr int JUDGE_COMPILE_TIMEOUT_SEC = 30;

constexpr int JUDGE_SANDBOX_MEMORY_KB = JUDGE_MEMORY_LIMIT_MB * 1024;

constexpr const char* JUDGE_TEMP_DIR = "./.judge-tmp/";
