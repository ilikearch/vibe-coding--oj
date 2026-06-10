#pragma once

#include <string>
#include <vector>

struct JudgeCase {
    std::string input;
    std::string expected;
};

struct JudgeResult {
    std::string status;
    std::string compile_error;
    int time_ms;
    int memory_kb;
    int failed_case;
    std::string expected_output;
    std::string actual_output;
};

JudgeResult compile_and_judge(const std::string& code, const std::vector<JudgeCase>& cases);
