#include <gtest/gtest.h>
#include "auth.h"

TEST(BcryptTest, HashNonEmpty) {
    std::string hash = bcrypt_hash("password123");
    EXPECT_FALSE(hash.empty());
    EXPECT_NE(hash.find("$2a$"), std::string::npos);
}

TEST(BcryptTest, HashIsDeterministicForSameInput) {
    std::string h1 = bcrypt_hash("hello");
    std::string h2 = bcrypt_hash("hello");
    EXPECT_NE(h1, h2);
}

TEST(BcryptTest, VerifyCorrectPassword) {
    std::string hash = bcrypt_hash("mypassword");
    EXPECT_TRUE(bcrypt_verify("mypassword", hash));
}

TEST(BcryptTest, VerifyWrongPassword) {
    std::string hash = bcrypt_hash("correct");
    EXPECT_FALSE(bcrypt_verify("wrong", hash));
}

TEST(BcryptTest, VerifyEmptyPassword) {
    std::string hash = bcrypt_hash("");
    EXPECT_TRUE(bcrypt_verify("", hash));
    EXPECT_FALSE(bcrypt_verify("x", hash));
}

TEST(BcryptTest, LongPassword) {
    std::string pass(100, 'x');
    std::string hash = bcrypt_hash(pass);
    EXPECT_TRUE(bcrypt_verify(pass, hash));
}

TEST(SessionTest, GenerateTokenNonEmpty) {
    std::string token = generate_session_token();
    EXPECT_FALSE(token.empty());
    EXPECT_EQ(token.size(), 32u);
}

TEST(SessionTest, TokensAreUnique) {
    std::string t1 = generate_session_token();
    std::string t2 = generate_session_token();
    EXPECT_NE(t1, t2);
}

TEST(SessionTest, CreateAndGetSession) {
    std::string token = create_session(1, "alice", "user");
    EXPECT_FALSE(token.empty());
    Session* s = get_session(token);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->user_id, 1);
    EXPECT_EQ(s->username, "alice");
    EXPECT_EQ(s->role, "user");
    EXPECT_NE(s->created_at, 0);
}

TEST(SessionTest, GetNonexistentSession) {
    Session* s = get_session("nonexistent_token_xyz");
    EXPECT_EQ(s, nullptr);
}

TEST(SessionTest, DestroySession) {
    std::string token = create_session(999, "test", "user");
    EXPECT_NE(get_session(token), nullptr);
    destroy_session(token);
    EXPECT_EQ(get_session(token), nullptr);
}

TEST(CookieTest, ParseSingleCookie) {
    std::string val = get_cookie("session_id=abc123", "session_id");
    EXPECT_EQ(val, "abc123");
}

TEST(CookieTest, ParseMultipleCookies) {
    std::string val = get_cookie("a=1; session_id=xyz789; b=2", "session_id");
    EXPECT_EQ(val, "xyz789");
}

TEST(CookieTest, KeyNotFound) {
    std::string val = get_cookie("a=1; b=2", "session_id");
    EXPECT_EQ(val, "");
}

TEST(CookieTest, EmptyCookieHeader) {
    std::string val = get_cookie("", "session_id");
    EXPECT_EQ(val, "");
}

TEST(CookieTest, ValueWithEquals) {
    std::string val = get_cookie("token=abc=def; other=x", "token");
    EXPECT_EQ(val, "abc=def");
}

TEST(CookieTest, TrimLeadingSpaces) {
    std::string val = get_cookie(" a=1 ; b=2", "b");
    EXPECT_EQ(val, "2");
}
