#include <gtest/gtest.h>
#include "config.h"

TEST(ConfigTest, JudgeConstantsSanity) {
    EXPECT_GT(JUDGE_CPU_TIMEOUT_SEC, 0);
    EXPECT_GT(JUDGE_REAL_TIMEOUT_MS, 0);
    EXPECT_GT(JUDGE_MEMORY_LIMIT_MB, 0);
    EXPECT_GT(JUDGE_OUTPUT_LIMIT_MB, 0);
    EXPECT_GT(JUDGE_COMPILE_TIMEOUT_SEC, 0);
    EXPECT_GT(JUDGE_SANDBOX_MEMORY_KB, 0);
}

TEST(ConfigTest, JudgeMemoryConversion) {
    EXPECT_EQ(JUDGE_SANDBOX_MEMORY_KB, JUDGE_MEMORY_LIMIT_MB * 1024);
}

TEST(ConfigTest, TempDirEndsWithSlash) {
    std::string dir = JUDGE_TEMP_DIR;
    EXPECT_EQ(dir.back(), '/');
}
