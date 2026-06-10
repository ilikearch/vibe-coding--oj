#include <gtest/gtest.h>
#include "db.h"
#include <stdexcept>

class DbTest : public ::testing::Test {
protected:
    void SetUp() override {
        db = new Database();
        db->query("DELETE FROM test_cases");
        db->query("DELETE FROM problems");
        db->query("DELETE FROM users");
        db->query("ALTER TABLE problems AUTO_INCREMENT = 1");
        db->query("ALTER TABLE test_cases AUTO_INCREMENT = 1");
        db->query("ALTER TABLE users AUTO_INCREMENT = 1");
    }

    void TearDown() override {
        delete db;
    }

    Database* db;
};

TEST_F(DbTest, ConnectSucceeds) {
    EXPECT_NE(db->conn(), nullptr);
}

TEST_F(DbTest, CountProblemsInitiallyZero) {
    EXPECT_EQ(db->count_problems(), 0);
}

TEST_F(DbTest, CountUsersInitiallyZero) {
    EXPECT_EQ(db->count_users(), 0);
}

TEST_F(DbTest, InsertProblem) {
    int id = db->insert_problem("Two Sum", "Easy", "Find two numbers", "#include <iostream>");
    EXPECT_GT(id, 0);
    EXPECT_EQ(db->count_problems(), 1);
}

TEST_F(DbTest, GetProblemById) {
    int id = db->insert_problem("Two Sum", "Easy", "Find two numbers", "#include <iostream>");
    Problem p = db->get_problem(id);
    EXPECT_EQ(p.id, id);
    EXPECT_EQ(p.title, "Two Sum");
    EXPECT_EQ(p.difficulty, "Easy");
    EXPECT_EQ(p.content, "Find two numbers");
    EXPECT_EQ(p.template_code, "#include <iostream>");
}

TEST_F(DbTest, GetNonexistentProblemReturnsEmpty) {
    Problem p = db->get_problem(9999);
    EXPECT_EQ(p.id, 0);
}

TEST_F(DbTest, UpdateProblem) {
    int id = db->insert_problem("Old", "Easy", "Old content", "");
    db->update_problem(id, "New", "Hard", "New content", "template");
    Problem p = db->get_problem(id);
    EXPECT_EQ(p.title, "New");
    EXPECT_EQ(p.difficulty, "Hard");
    EXPECT_EQ(p.content, "New content");
    EXPECT_EQ(p.template_code, "template");
}

TEST_F(DbTest, DeleteProblem) {
    int id = db->insert_problem("ToDelete", "Easy", "content", "");
    EXPECT_EQ(db->count_problems(), 1);
    db->delete_problem(id);
    EXPECT_EQ(db->count_problems(), 0);
}

TEST_F(DbTest, GetAllProblems) {
    db->insert_problem("A", "Easy", "a", "");
    db->insert_problem("B", "Medium", "b", "");
    db->insert_problem("C", "Hard", "c", "");
    auto problems = db->get_all_problems();
    EXPECT_EQ(problems.size(), 3u);
    EXPECT_EQ(problems[0].title, "A");
    EXPECT_EQ(problems[1].title, "B");
    EXPECT_EQ(problems[2].title, "C");
}

TEST_F(DbTest, InsertTestCase) {
    int pid = db->insert_problem("P", "Easy", "desc", "");
    int id = db->insert_test_case(pid, "1 2", "3", 0);
    EXPECT_GT(id, 0);
}

TEST_F(DbTest, GetTestCases) {
    int pid = db->insert_problem("P", "Easy", "desc", "");
    db->insert_test_case(pid, "1 2", "3", 0);
    db->insert_test_case(pid, "4 5", "9", 1);
    auto cases = db->get_test_cases(pid);
    ASSERT_EQ(cases.size(), 2u);
    EXPECT_EQ(cases[0].input, "1 2");
    EXPECT_EQ(cases[0].expected, "3");
    EXPECT_EQ(cases[0].position, 0);
    EXPECT_EQ(cases[1].input, "4 5");
    EXPECT_EQ(cases[1].expected, "9");
    EXPECT_EQ(cases[1].position, 1);
}

TEST_F(DbTest, DeleteTestCase) {
    int pid = db->insert_problem("P", "Easy", "desc", "");
    int tid = db->insert_test_case(pid, "1", "2", 0);
    auto cases = db->get_test_cases(pid);
    EXPECT_EQ(cases.size(), 1u);
    db->delete_test_case(tid);
    cases = db->get_test_cases(pid);
    EXPECT_EQ(cases.size(), 0u);
}

TEST_F(DbTest, CascadeDeleteProblemRemovesTestCases) {
    int pid = db->insert_problem("P", "Easy", "desc", "");
    db->insert_test_case(pid, "1", "2", 0);
    db->insert_test_case(pid, "3", "4", 1);
    EXPECT_EQ(db->get_test_cases(pid).size(), 2u);
    db->delete_problem(pid);
    EXPECT_EQ(db->get_test_cases(pid).size(), 0u);
}

TEST_F(DbTest, InsertUser) {
    int id = db->insert_user("alice", "hashed", "user");
    EXPECT_GT(id, 0);
    EXPECT_EQ(db->count_users(), 1);
}

TEST_F(DbTest, GetUserByUsername) {
    db->insert_user("bob", "hash1", "user");
    User u = db->get_user_by_username("bob");
    EXPECT_EQ(u.username, "bob");
    EXPECT_EQ(u.password, "hash1");
    EXPECT_EQ(u.role, "user");
}

TEST_F(DbTest, GetUserById) {
    int id = db->insert_user("charlie", "hash2", "admin");
    User u = db->get_user_by_id(id);
    EXPECT_EQ(u.username, "charlie");
    EXPECT_EQ(u.role, "admin");
}

TEST_F(DbTest, GetNonexistentUser) {
    User u = db->get_user_by_username("nobody");
    EXPECT_EQ(u.id, 0);
}

TEST_F(DbTest, GetAllUsers) {
    db->insert_user("u1", "h1", "user");
    db->insert_user("u2", "h2", "admin");
    auto users = db->get_all_users();
    EXPECT_EQ(users.size(), 2u);
}

TEST_F(DbTest, SpecialCharactersInContent) {
    int pid = db->insert_problem("P", "Easy", "a < b && b > c", "");
    Problem p = db->get_problem(pid);
    EXPECT_EQ(p.content, "a < b && b > c");
}

TEST_F(DbTest, SpecialCharactersInTestCase) {
    int pid = db->insert_problem("P", "Easy", "desc", "");
    db->insert_test_case(pid, "hello 'world'", "\"quoted\"", 0);
    auto cases = db->get_test_cases(pid);
    EXPECT_EQ(cases[0].input, "hello 'world'");
    EXPECT_EQ(cases[0].expected, "\"quoted\"");
}
