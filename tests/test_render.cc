#include <gtest/gtest.h>
#include "render.h"

TEST(RenderTest, ReplaceBasic) {
    std::string tmpl = "Hello {{NAME}}!";
    std::string result = replace(tmpl, "NAME", "World");
    EXPECT_EQ(result, "Hello World!");
}

TEST(RenderTest, ReplaceMultipleOccurrences) {
    std::string tmpl = "{{X}} and {{X}} and {{X}}";
    std::string result = replace(tmpl, "X", "Y");
    EXPECT_EQ(result, "Y and Y and Y");
}

TEST(RenderTest, ReplaceNoMatch) {
    std::string tmpl = "Hello world";
    std::string result = replace(tmpl, "NOPE", "yes");
    EXPECT_EQ(result, "Hello world");
}

TEST(RenderTest, ReplaceAll) {
    std::string tmpl = "Hi {{A}}, your score is {{B}}/{{C}}";
    std::map<std::string, std::string> vars = {
        {"A", "Alice"},
        {"B", "95"},
        {"C", "100"}
    };
    std::string result = replace_all(tmpl, vars);
    EXPECT_EQ(result, "Hi Alice, your score is 95/100");
}

TEST(RenderTest, RenderPage) {
    std::string body = "<h1>Content</h1>";
    std::string nav = "Home | Login";
    std::string page = render_page("Test Page", body, nav);
    EXPECT_NE(page.find("Test Page - Vibe OJ"), std::string::npos);
    EXPECT_NE(page.find("<h1>Content</h1>"), std::string::npos);
    EXPECT_NE(page.find("Home | Login"), std::string::npos);
    EXPECT_NE(page.find("<!DOCTYPE html>"), std::string::npos);
    EXPECT_NE(page.find("</html>"), std::string::npos);
}

TEST(RenderTest, ReplaceEmptyValue) {
    std::string tmpl = "[{{VAL}}]";
    std::string result = replace(tmpl, "VAL", "");
    EXPECT_EQ(result, "[]");
}

TEST(RenderTest, ReplaceWithBracesInValue) {
    std::string tmpl = "{{CODE}}";
    std::string result = replace(tmpl, "CODE", "if (x < 0) return;");
    EXPECT_EQ(result, "if (x < 0) return;");
}

TEST(RenderTest, ReplaceNonexistentKeyIsNoop) {
    std::string tmpl = "Hello {{NAME}}";
    std::map<std::string, std::string> vars = {{"OTHER", "value"}};
    std::string result = replace_all(tmpl, vars);
    EXPECT_EQ(result, "Hello {{NAME}}");
}
