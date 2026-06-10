#pragma once

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>

struct Problem {
    int id;
    std::string title;
    std::string difficulty;
    std::string content;
    std::string template_code;
    std::string created_at;
};

struct TestCase {
    int id;
    int problem_id;
    std::string input;
    std::string expected;
    int position;
};

struct User {
    int id;
    std::string username;
    std::string password;
    std::string role;
    std::string created_at;
};

class Database {
public:
    Database();
    ~Database();

    MYSQL* conn() { return conn_; }

    Problem get_problem(int id);
    std::vector<Problem> get_all_problems();
    int insert_problem(const std::string& title, const std::string& difficulty,
                       const std::string& content, const std::string& template_code);
    void update_problem(int id, const std::string& title, const std::string& difficulty,
                        const std::string& content, const std::string& template_code);
    void delete_problem(int id);

    std::vector<TestCase> get_test_cases(int problem_id);
    int insert_test_case(int problem_id, const std::string& input,
                         const std::string& expected, int position);
    void delete_test_case(int id);

    User get_user_by_username(const std::string& username);
    User get_user_by_id(int id);
    std::vector<User> get_all_users();
    int insert_user(const std::string& username, const std::string& password,
                    const std::string& role = "user");
    int count_problems();
    int count_users();

    MYSQL_RES* query(const std::string& sql);

private:
    MYSQL* conn_;
    std::string escape(const std::string& str);
};
