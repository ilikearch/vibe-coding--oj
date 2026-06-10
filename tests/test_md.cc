#include <gtest/gtest.h>
#include "md.h"

TEST(MdTest, HeadingH1) {
    std::string html = md_to_html("# Title");
    EXPECT_NE(html.find("<h1>Title</h1>"), std::string::npos);
}

TEST(MdTest, HeadingH3) {
    std::string html = md_to_html("### Subtitle");
    EXPECT_NE(html.find("<h3>Subtitle</h3>"), std::string::npos);
}

TEST(MdTest, HeadingH6) {
    std::string html = md_to_html("###### Tiny");
    EXPECT_NE(html.find("<h6>Tiny</h6>"), std::string::npos);
}

TEST(MdTest, NotHeadingWithoutSpace) {
    std::string html = md_to_html("###NoSpace");
    EXPECT_EQ(html.find("<h"), std::string::npos);
}

TEST(MdTest, Bold) {
    std::string html = md_to_html("This is **bold** text.");
    EXPECT_NE(html.find("<strong>bold</strong>"), std::string::npos);
}

TEST(MdTest, CodeBlock) {
    std::string md = "```\nint x = 1;\n```";
    std::string html = md_to_html(md);
    EXPECT_NE(html.find("<pre><code>"), std::string::npos);
    EXPECT_NE(html.find("int x = 1;"), std::string::npos);
    EXPECT_NE(html.find("</code></pre>"), std::string::npos);
}

TEST(MdTest, CodeBlockEscapesHTML) {
    std::string md = "```\n#include <vector>\n```";
    std::string html = md_to_html(md);
    EXPECT_NE(html.find("&lt;vector&gt;"), std::string::npos);
}

TEST(MdTest, UnorderedList) {
    std::string md = "- item1\n- item2";
    std::string html = md_to_html(md);
    EXPECT_NE(html.find("<ul>"), std::string::npos);
    EXPECT_NE(html.find("<li>item1</li>"), std::string::npos);
    EXPECT_NE(html.find("<li>item2</li>"), std::string::npos);
    EXPECT_NE(html.find("</ul>"), std::string::npos);
}

TEST(MdTest, Paragraph) {
    std::string html = md_to_html("Just a paragraph.");
    EXPECT_NE(html.find("<p>Just a paragraph.</p>"), std::string::npos);
}

TEST(MdTest, MixedContent) {
    std::string md = "# Title\n\nSome **bold** text\n\n- item";
    std::string html = md_to_html(md);
    EXPECT_NE(html.find("<h1>Title</h1>"), std::string::npos);
    EXPECT_NE(html.find("<strong>bold</strong>"), std::string::npos);
    EXPECT_NE(html.find("<li>item</li>"), std::string::npos);
}

TEST(MdTest, EmptyInput) {
    std::string html = md_to_html("");
    EXPECT_EQ(html, "");
}

TEST(MdTest, SevenHashesIsParagraph) {
    std::string html = md_to_html("####### not heading");
    EXPECT_EQ(html.find("<h7>"), std::string::npos);
}

TEST(MdTest, HtmlEscapingInParagraph) {
    std::string html = md_to_html("Use <div> tags");
    EXPECT_NE(html.find("&lt;div&gt;"), std::string::npos);
}

TEST(MdTest, MultipleBolds) {
    std::string html = md_to_html("**a** and **b**");
    EXPECT_NE(html.find("<strong>a</strong>"), std::string::npos);
    EXPECT_NE(html.find("<strong>b</strong>"), std::string::npos);
}
