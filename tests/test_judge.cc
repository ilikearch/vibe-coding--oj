#include <gtest/gtest.h>
#include "judge.h"

TEST(JudgeTest, AcSimpleSum) {
    std::string code = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a + b << std::endl;
    return 0;
})";
    JudgeCase tc;
    tc.input = "2 3\n";
    tc.expected = "5\n";
    JudgeResult r = compile_and_judge(code, {tc});
    EXPECT_EQ(r.status, "AC");
    EXPECT_GT(r.time_ms, 0);
}

TEST(JudgeTest, AcMultiLineOutput) {
    std::string code = R"(#include <iostream>
int main() {
    std::cout << "Hello\nWorld" << std::endl;
    return 0;
})";
    JudgeCase tc;
    tc.input = "";
    tc.expected = "Hello\nWorld\n";
    JudgeResult r = compile_and_judge(code, {tc});
    EXPECT_EQ(r.status, "AC");
}

TEST(JudgeTest, AcNoOutput) {
    std::string code = R"(int main() { return 0; })";
    JudgeCase tc;
    tc.input = "";
    tc.expected = "";
    JudgeResult r = compile_and_judge(code, {tc});
    EXPECT_EQ(r.status, "AC");
}

TEST(JudgeTest, WrongAnswer) {
    std::string code = R"(#include <iostream>
int main() {
    std::cout << "999" << std::endl;
    return 0;
})";
    JudgeCase tc;
    tc.input = "";
    tc.expected = "42\n";
    JudgeResult r = compile_and_judge(code, {tc});
    EXPECT_EQ(r.status, "WA");
    EXPECT_EQ(r.failed_case, 1);
    EXPECT_EQ(r.expected_output, "42");
    EXPECT_EQ(r.actual_output, "999");
}

TEST(JudgeTest, WaOnSecondCase) {
    std::string code = R"(#include <iostream>
int main() {
    int x;
    std::cin >> x;
    std::cout << x * 2 << std::endl;
    return 0;
})";
    JudgeCase tc1; tc1.input = "5\n"; tc1.expected = "10\n";
    JudgeCase tc2; tc2.input = "7\n"; tc2.expected = "99\n";
    JudgeResult r = compile_and_judge(code, {tc1, tc2});
    EXPECT_EQ(r.status, "WA");
    EXPECT_EQ(r.failed_case, 2);
}

TEST(JudgeTest, CompileError) {
    std::string code = "this is not valid c++ code at all !!!";
    JudgeCase tc; tc.input = ""; tc.expected = "";
    JudgeResult r = compile_and_judge(code, {tc});
    EXPECT_EQ(r.status, "CE");
    EXPECT_FALSE(r.compile_error.empty());
}

TEST(JudgeTest, CompileErrorSyntax) {
    std::string code = R"(#include <iostream>
int main() {
    std::cout << "no semicolon"
})";
    JudgeCase tc; tc.input = ""; tc.expected = "";
    JudgeResult r = compile_and_judge(code, {tc});
    EXPECT_EQ(r.status, "CE");
}

TEST(JudgeTest, TimeLimitExceeded) {
    std::string code = R"(int main() { while(1) {} return 0; })";
    JudgeCase tc; tc.input = ""; tc.expected = "";
    JudgeResult r = compile_and_judge(code, {tc});
    EXPECT_TRUE(r.status == "TLE" || r.status == "RE");
}

TEST(JudgeTest, RuntimeErrorSegfault) {
    std::string code = R"(int main() { int* p = nullptr; *p = 42; return 0; })";
    JudgeCase tc; tc.input = ""; tc.expected = "";
    JudgeResult r = compile_and_judge(code, {tc});
    EXPECT_EQ(r.status, "RE");
}

TEST(JudgeTest, RuntimeErrorAbort) {
    std::string code = R"(#include <cstdlib>
int main() { std::abort(); return 0; })";
    JudgeCase tc; tc.input = ""; tc.expected = "";
    JudgeResult r = compile_and_judge(code, {tc});
    EXPECT_EQ(r.status, "RE");
}

TEST(JudgeTest, MemoryLimitExceeded) {
    std::string code = R"(#include <cstdlib>
#include <cstring>
int main() {
    size_t alloced = 0;
    while (alloced < 512UL * 1024 * 1024) {
        size_t chunk = 64UL * 1024 * 1024;
        void* p = malloc(chunk);
        if (!p) break;
        memset(p, 0, chunk);
        alloced += chunk;
    }
    while (1) {}
    return 0;
})";
    JudgeCase tc; tc.input = ""; tc.expected = "";
    JudgeResult r = compile_and_judge(code, {tc});
    EXPECT_TRUE(r.status == "MLE" || r.status == "TLE" || r.status == "RE");
}

TEST(JudgeTest, OutputTrimTrailingWhitespace) {
    std::string code = R"(#include <iostream>
int main() {
    std::cout << "42   \n\n\n" << std::endl;
    return 0;
})";
    JudgeCase tc;
    tc.input = "";
    tc.expected = "42";
    JudgeResult r = compile_and_judge(code, {tc});
    EXPECT_EQ(r.status, "AC");
}

TEST(JudgeTest, EmptyTestCaseInput) {
    std::string code = R"(#include <iostream>
#include <string>
int main() {
    std::string s;
    std::getline(std::cin, s);
    std::cout << s << std::endl;
    return 0;
})";
    JudgeCase tc;
    tc.input = "\n";
    tc.expected = "\n";
    JudgeResult r = compile_and_judge(code, {tc});
    EXPECT_EQ(r.status, "AC");
}

TEST(JudgeTest, NoTestCases) {
    std::string code = R"(int main() { return 0; })";
    std::vector<JudgeCase> empty_cases;
    JudgeResult r = compile_and_judge(code, empty_cases);
    EXPECT_EQ(r.status, "CE");
    EXPECT_FALSE(r.compile_error.empty());
}

TEST(JudgeTest, MultipleTestCasesAllAc) {
    std::string code = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a + b << std::endl;
    return 0;
})";
    std::vector<JudgeCase> cases;
    for (int i = 0; i < 5; i++) {
        JudgeCase tc;
        tc.input = std::to_string(i) + " " + std::to_string(i * 2) + "\n";
        tc.expected = std::to_string(i + i * 2) + "\n";
        cases.push_back(tc);
    }
    JudgeResult r = compile_and_judge(code, cases);
    EXPECT_EQ(r.status, "AC");
}
