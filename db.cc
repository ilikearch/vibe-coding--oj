#include "db.h"
#include "config.h"
#include "log.h"
#include <cstring>
#include <sstream>

Database::Database() {
    conn_ = mysql_init(nullptr);
    if (!conn_) throw std::runtime_error("mysql_init failed");
    if (!mysql_real_connect(conn_, DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT, nullptr, 0)) {
        std::string err = mysql_error(conn_);
        mysql_close(conn_);
        LOG_ERROR("DB connect failed: " + err);
        throw std::runtime_error("mysql_real_connect failed: " + err);
    }
    LOG_INFO("DB connected: " + std::string(DB_HOST) + "/" + DB_NAME);
}

Database::~Database() {
    if (conn_) mysql_close(conn_);
}

std::string Database::escape(const std::string& str) {
    if (str.empty()) return "";
    std::vector<char> buf(str.size() * 2 + 1);
    mysql_real_escape_string(conn_, buf.data(), str.c_str(), str.size());
    return std::string(buf.data());
}

MYSQL_RES* Database::query(const std::string& sql) {
    if (mysql_query(conn_, sql.c_str()) != 0) {
        std::string err = std::string(mysql_error(conn_)) + " | SQL: " + sql;
        LOG_ERROR("DB query failed: " + err);
        throw std::runtime_error("query failed: " + err);
    }
    return mysql_store_result(conn_);
}

Problem Database::get_problem(int id) {
    std::ostringstream ss;
    ss << "SELECT id, title, difficulty, content, template, created_at FROM problems WHERE id=" << id;
    MYSQL_RES* res = query(ss.str());
    if (!res) return {};
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) { mysql_free_result(res); return {}; }
    Problem p;
    p.id = row[0] ? std::stoi(row[0]) : 0;
    p.title = row[1] ? row[1] : "";
    p.difficulty = row[2] ? row[2] : "";
    p.content = row[3] ? row[3] : "";
    p.template_code = row[4] ? row[4] : "";
    p.created_at = row[5] ? row[5] : "";
    mysql_free_result(res);
    return p;
}

std::vector<Problem> Database::get_all_problems() {
    MYSQL_RES* res = query("SELECT id, title, difficulty, content, template, created_at FROM problems ORDER BY id");
    std::vector<Problem> problems;
    if (!res) return problems;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        Problem p;
        p.id = row[0] ? std::stoi(row[0]) : 0;
        p.title = row[1] ? row[1] : "";
        p.difficulty = row[2] ? row[2] : "";
        p.content = row[3] ? row[3] : "";
        p.template_code = row[4] ? row[4] : "";
        p.created_at = row[5] ? row[5] : "";
        problems.push_back(p);
    }
    mysql_free_result(res);
    return problems;
}

int Database::insert_problem(const std::string& title, const std::string& difficulty,
                             const std::string& content, const std::string& template_code) {
    std::ostringstream ss;
    ss << "INSERT INTO problems (title, difficulty, content, template) VALUES ('"
       << escape(title) << "', '" << escape(difficulty) << "', '"
       << escape(content) << "', '" << escape(template_code) << "')";
    query(ss.str());
    return mysql_insert_id(conn_);
}

void Database::update_problem(int id, const std::string& title, const std::string& difficulty,
                              const std::string& content, const std::string& template_code) {
    std::ostringstream ss;
    ss << "UPDATE problems SET title='" << escape(title) << "', difficulty='"
       << escape(difficulty) << "', content='" << escape(content)
       << "', template='" << escape(template_code) << "' WHERE id=" << id;
    query(ss.str());
}

void Database::delete_problem(int id) {
    std::ostringstream ss;
    ss << "DELETE FROM problems WHERE id=" << id;
    query(ss.str());
}

std::vector<TestCase> Database::get_test_cases(int problem_id) {
    std::ostringstream ss;
    ss << "SELECT id, problem_id, input, expected, position FROM test_cases WHERE problem_id="
       << problem_id << " ORDER BY position, id";
    MYSQL_RES* res = query(ss.str());
    std::vector<TestCase> cases;
    if (!res) return cases;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        TestCase tc;
        tc.id = row[0] ? std::stoi(row[0]) : 0;
        tc.problem_id = row[1] ? std::stoi(row[1]) : 0;
        tc.input = row[2] ? row[2] : "";
        tc.expected = row[3] ? row[3] : "";
        tc.position = row[4] ? std::stoi(row[4]) : 0;
        cases.push_back(tc);
    }
    mysql_free_result(res);
    return cases;
}

int Database::insert_test_case(int problem_id, const std::string& input,
                               const std::string& expected, int position) {
    std::ostringstream ss;
    ss << "INSERT INTO test_cases (problem_id, input, expected, position) VALUES ("
       << problem_id << ", '" << escape(input) << "', '" << escape(expected)
       << "', " << position << ")";
    query(ss.str());
    return mysql_insert_id(conn_);
}

void Database::delete_test_case(int id) {
    std::ostringstream ss;
    ss << "DELETE FROM test_cases WHERE id=" << id;
    query(ss.str());
}

User Database::get_user_by_username(const std::string& username) {
    std::ostringstream ss;
    ss << "SELECT id, username, password, role, created_at FROM users WHERE username='"
       << escape(username) << "'";
    MYSQL_RES* res = query(ss.str());
    if (!res) return {};
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) { mysql_free_result(res); return {}; }
    User u;
    u.id = row[0] ? std::stoi(row[0]) : 0;
    u.username = row[1] ? row[1] : "";
    u.password = row[2] ? row[2] : "";
    u.role = row[3] ? row[3] : "";
    u.created_at = row[4] ? row[4] : "";
    mysql_free_result(res);
    return u;
}

User Database::get_user_by_id(int id) {
    std::ostringstream ss;
    ss << "SELECT id, username, password, role, created_at FROM users WHERE id=" << id;
    MYSQL_RES* res = query(ss.str());
    if (!res) return {};
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) { mysql_free_result(res); return {}; }
    User u;
    u.id = row[0] ? std::stoi(row[0]) : 0;
    u.username = row[1] ? row[1] : "";
    u.password = row[2] ? row[2] : "";
    u.role = row[3] ? row[3] : "";
    u.created_at = row[4] ? row[4] : "";
    mysql_free_result(res);
    return u;
}

std::vector<User> Database::get_all_users() {
    MYSQL_RES* res = query("SELECT id, username, password, role, created_at FROM users ORDER BY id");
    std::vector<User> users;
    if (!res) return users;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        User u;
        u.id = row[0] ? std::stoi(row[0]) : 0;
        u.username = row[1] ? row[1] : "";
        u.password = row[2] ? row[2] : "";
        u.role = row[3] ? row[3] : "";
        u.created_at = row[4] ? row[4] : "";
        users.push_back(u);
    }
    mysql_free_result(res);
    return users;
}

int Database::insert_user(const std::string& username, const std::string& password,
                          const std::string& role) {
    std::ostringstream ss;
    ss << "INSERT INTO users (username, password, role) VALUES ('"
       << escape(username) << "', '" << escape(password) << "', '"
       << escape(role) << "')";
    query(ss.str());
    return mysql_insert_id(conn_);
}

int Database::count_problems() {
    MYSQL_RES* res = query("SELECT COUNT(*) FROM problems");
    if (!res) return 0;
    MYSQL_ROW row = mysql_fetch_row(res);
    int count = row && row[0] ? std::stoi(row[0]) : 0;
    mysql_free_result(res);
    return count;
}

int Database::count_users() {
    MYSQL_RES* res = query("SELECT COUNT(*) FROM users");
    if (!res) return 0;
    MYSQL_ROW row = mysql_fetch_row(res);
    int count = row && row[0] ? std::stoi(row[0]) : 0;
    mysql_free_result(res);
    return count;
}
