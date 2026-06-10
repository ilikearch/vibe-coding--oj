#include "deps/cpp-httplib/httplib.h"
#include <nlohmann/json.hpp>
#include "config.h"
#include "db.h"
#include "render.h"
#include "auth.h"
#include "judge.h"
#include "md.h"
#include "log.h"
#include <iostream>
#include <sstream>

using json = nlohmann::json;
static Database db;

static std::string nav_public() {
    return "<a href=\"/\">Home</a> | <a href=\"/login\">Login</a> | <a href=\"/register\">Register</a>";
}
static std::string nav_user(const std::string& username, bool is_admin) {
    std::string s = "<a href=\"/problems\">Problems</a> | " + username + " | <a href=\"/logout\">Logout</a>";
    if (is_admin) s += " | <a href=\"/admin\">Admin</a>";
    return s;
}

static std::string load_tmpl(const std::string& name) {
    std::string tmpl = read_template("templates/" + name);
    return tmpl.empty() ? ("<!-- template not found: " + name + " -->") : tmpl;
}

static std::string build_table_rows(const std::vector<Problem>& problems) {
    if (problems.empty()) return "<tr><td colspan=\"3\">No problems yet</td></tr>";
    std::ostringstream ss;
    for (const auto& p : problems) {
        ss << "<tr>"
           << "<td>" << p.id << "</td>"
           << "<td><a href=\"/problem/" << p.id << "\">" << p.title << "</a></td>"
           << "<td><span class=\"" << p.difficulty << "\">" << p.difficulty << "</span></td>"
           << "</tr>";
    }
    return ss.str();
}

static std::string build_admin_rows(const std::vector<Problem>& problems) {
    if (problems.empty()) return "<tr><td colspan=\"4\">No problems yet</td></tr>";
    std::ostringstream ss;
    for (const auto& p : problems) {
        ss << "<tr>"
           << "<td>" << p.id << "</td>"
           << "<td>" << p.title << "</td>"
           << "<td><span class=\"" << p.difficulty << "\">" << p.difficulty << "</span></td>"
           << "<td>"
           << "<a href=\"/admin/problems/" << p.id << "/edit\">Edit</a> | "
           << "<a href=\"/admin/problems/" << p.id << "/testcases\">Test Cases</a> | "
           << "<button onclick=\"deleteProblem(" << p.id << ")\">Delete</button>"
           << "</td></tr>";
    }
    return ss.str();
}

static std::string build_user_rows(const std::vector<User>& users) {
    if (users.empty()) return "<tr><td colspan=\"4\">No users</td></tr>";
    std::ostringstream ss;
    for (const auto& u : users) {
        ss << "<tr>"
           << "<td>" << u.id << "</td>"
           << "<td>" << u.username << "</td>"
           << "<td>" << u.role << "</td>"
           << "<td>" << u.created_at << "</td>"
           << "</tr>";
    }
    return ss.str();
}

static std::string build_tc_rows(const std::vector<TestCase>& cases, int problem_id) {
    if (cases.empty()) return "<tr><td colspan=\"5\">No test cases yet</td></tr>";
    std::ostringstream ss;
    for (const auto& tc : cases) {
        ss << "<tr>"
           << "<td>" << tc.id << "</td>"
           << "<td>" << tc.position << "</td>"
           << "<td><pre>" << tc.input << "</pre></td>"
           << "<td><pre>" << tc.expected << "</pre></td>"
           << "<td><button onclick=\"deleteTestCase(" << tc.id << "," << problem_id << ")\">Delete</button></td>"
           << "</tr>";
    }
    return ss.str();
}

static std::string build_sample_cases(const std::vector<TestCase>& cases) {
    if (cases.empty()) return "";
    std::ostringstream ss;
    ss << "<h3>Sample Test Cases</h3>";
    for (size_t i = 0; i < cases.size(); i++) {
        ss << "<p><strong>Input:</strong></p><pre>" << cases[i].input << "</pre>"
           << "<p><strong>Expected:</strong></p><pre>" << cases[i].expected << "</pre>";
        if (i < cases.size() - 1) ss << "<hr>";
    }
    return ss.str();
}

static std::string build_difficulty_options(const std::string& selected) {
    std::ostringstream ss;
    for (const auto& d : {"Easy", "Medium", "Hard"}) {
        ss << "<option" << (d == selected ? " selected" : "") << ">" << d << "</option>";
    }
    return ss.str();
}

int main() {
    LOG_INFO("Vibe OJ Server starting on port 8080...");
    std::cout << "Vibe OJ Server starting on port 8080..." << std::endl;
    httplib::Server svr;

    svr.set_mount_point("/", "./static");

    // ==================== Public Routes ====================

    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        std::string sid = get_cookie(req.get_header_value("Cookie"), "session_id");
        Session* s = get_session(sid);
        std::string nav = s ? nav_user(s->username, s->role == "admin") : nav_public();
        int pcount = db.count_problems();
        int ucount = db.count_users();
        std::string tmpl = load_tmpl("landing.html");
        std::map<std::string, std::string> vars = {
            {"PROBLEM_COUNT", std::to_string(pcount)},
            {"USER_COUNT", std::to_string(ucount)},
        };
        std::string body = replace_all(tmpl, vars);
        res.set_content(render_page("Home", body, nav), "text/html");
    });

    svr.Get("/login", [](const httplib::Request&, httplib::Response& res) {
        std::string body = load_tmpl("login.html");
        res.set_content(render_page("Login", body, nav_public()), "text/html");
    });

    svr.Post("/login", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");
            User user = db.get_user_by_username(username);
            if (user.id == 0 || !bcrypt_verify(password, user.password)) {
                LOG_WARN("login failed: username=" + username);
                json resp = {{"success", false}, {"error", "Invalid credentials"}};
                res.set_content(resp.dump(), "application/json");
                return;
            }
            std::string token = create_session(user.id, user.username, user.role);
            LOG_INFO("login success: username=" + user.username);
            res.set_header("Set-Cookie", "session_id=" + token + "; Path=/; HttpOnly");
            json resp = {{"success", true}, {"redirect", "/problems"}};
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json resp = {{"success", false}, {"error", std::string("Bad request: ") + e.what()}};
            res.set_content(resp.dump(), "application/json");
        }
    });

    svr.Get("/register", [](const httplib::Request&, httplib::Response& res) {
        std::string body = load_tmpl("register.html");
        res.set_content(render_page("Register", body, nav_public()), "text/html");
    });

    svr.Post("/register", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");
            if (username.empty() || password.empty()) {
                json resp = {{"success", false}, {"error", "All fields required"}};
                res.set_content(resp.dump(), "application/json");
                return;
            }
            User existing = db.get_user_by_username(username);
            if (existing.id != 0) {
                json resp = {{"success", false}, {"error", "Username already taken"}};
                res.set_content(resp.dump(), "application/json");
                return;
            }
            std::string hash = bcrypt_hash(password);
            db.insert_user(username, hash);
            LOG_INFO("user registered: username=" + username);
            json resp = {{"success", true}, {"redirect", "/login"}};
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json resp = {{"success", false}, {"error", std::string("Bad request: ") + e.what()}};
            res.set_content(resp.dump(), "application/json");
        }
    });

    svr.Get("/logout", [](const httplib::Request& req, httplib::Response& res) {
        std::string sid = get_cookie(req.get_header_value("Cookie"), "session_id");
        Session* s = get_session(sid);
        if (s) LOG_INFO("logout: username=" + s->username);
        destroy_session(sid);
        res.set_header("Set-Cookie", "session_id=; Path=/; HttpOnly; Max-Age=0");
        res.set_redirect("/");
    });

    // ==================== User Routes ====================

    svr.Get("/problems", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        auto problems = db.get_all_problems();
        std::string tmpl = load_tmpl("problem_list.html");
        std::map<std::string, std::string> vars = {
            {"PROBLEM_ROWS", build_table_rows(problems)},
        };
        std::string body = replace_all(tmpl, vars);
        res.set_content(render_page("Problems", body, nav_user(session->username, session->role == "admin")), "text/html");
    });

    svr.Get(R"(/problem/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        int id = std::stoi(req.matches[1]);
        Problem p = db.get_problem(id);
        if (p.id == 0) { res.status = 404; res.set_content("<h1>404 Not Found</h1>", "text/html"); return; }
        auto cases = db.get_test_cases(id);
        std::string tmpl = load_tmpl("problem_detail.html");
        std::map<std::string, std::string> vars = {
            {"TITLE", p.title},
            {"DIFFICULTY", p.difficulty},
            {"DESCRIPTION", md_to_html(p.content)},
            {"TEMPLATE", p.template_code},
            {"ID", std::to_string(id)},
            {"SAMPLE_CASES", build_sample_cases(cases)},
            {"RESULT", ""},
        };
        std::string body = replace_all(tmpl, vars);
        res.set_content(render_page(p.title, body, nav_user(session->username, session->role == "admin")), "text/html");
    });

    svr.Post(R"(/problem/(\d+)/submit)", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH_JSON(req, res);
        try {
            json body = json::parse(req.body);
            std::string code = body.value("code", "");
            int id = std::stoi(req.matches[1]);
            if (code.empty()) {
                json resp = {{"success", false}, {"error", "Code is required"}};
                res.status = 400;
                res.set_content(resp.dump(), "application/json");
                return;
            }
            auto cases = db.get_test_cases(id);
            if (cases.empty()) {
                json resp = {{"success", false}, {"error", "No test cases for this problem"}};
                res.set_content(resp.dump(), "application/json");
                return;
            }
            std::vector<JudgeCase> judge_cases;
            for (const auto& tc : cases) {
                JudgeCase jc;
                jc.input = tc.input;
                jc.expected = tc.expected;
                judge_cases.push_back(jc);
            }
            JudgeResult jr = compile_and_judge(code, judge_cases);
            json resp = {
                {"status", jr.status},
                {"time_ms", jr.time_ms},
                {"memory_kb", jr.memory_kb},
                {"failed_case", jr.failed_case},
                {"expected_output", jr.expected_output},
                {"actual_output", jr.actual_output},
                {"compile_error", jr.compile_error}
            };
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json resp = {{"success", false}, {"error", std::string("Bad request: ") + e.what()}};
            res.set_content(resp.dump(), "application/json");
        }
    });

    // ==================== Admin Routes ====================

    svr.Get("/admin", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        CHECK_ADMIN(res);
        auto problems = db.get_all_problems();
        std::string tmpl = load_tmpl("admin_panel.html");
        std::map<std::string, std::string> vars = {
            {"PROBLEM_ROWS", build_admin_rows(problems)},
        };
        std::string body = replace_all(tmpl, vars);
        res.set_content(render_page("Admin Panel", body, nav_user(session->username, true)), "text/html");
    });

    svr.Get("/admin/problems/new", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        CHECK_ADMIN(res);
        std::string tmpl = load_tmpl("admin_problem_form.html");
        std::map<std::string, std::string> vars = {
            {"TITLE", "New"},
            {"TITLE_VALUE", ""},
            {"DIFFICULTY_OPTIONS", build_difficulty_options("Easy")},
            {"CONTENT", ""},
            {"TEMPLATE", ""},
            {"SUBMIT_FN", "createProblem"},
            {"SUBMIT_LABEL", "Create"},
            {"ID", "0"},
        };
        std::string body = replace_all(tmpl, vars);
        res.set_content(render_page("New Problem", body, nav_user(session->username, true)), "text/html");
    });

    svr.Post("/admin/problems", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH_JSON(req, res);
        CHECK_ADMIN_JSON(res);
        try {
            json body = json::parse(req.body);
            std::string title = body.value("title", "");
            std::string difficulty = body.value("difficulty", "");
            std::string content = body.value("content", "");
            std::string template_code = body.value("template", "");
            if (title.empty() || difficulty.empty() || content.empty()) {
                json resp = {{"success", false}, {"error", "Title, difficulty, and content are required"}};
                res.status = 400;
                res.set_content(resp.dump(), "application/json");
                return;
            }
            int id = db.insert_problem(title, difficulty, content, template_code);
            LOG_INFO("problem created: id=" + std::to_string(id) + " title=" + title);
            json resp = {{"success", true}, {"id", id}, {"redirect", "/admin"}};
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json resp = {{"success", false}, {"error", std::string("Bad request: ") + e.what()}};
            res.set_content(resp.dump(), "application/json");
        }
    });

    svr.Get(R"(/admin/problems/(\d+)/edit)", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        CHECK_ADMIN(res);
        int id = std::stoi(req.matches[1]);
        Problem p = db.get_problem(id);
        if (p.id == 0) { res.status = 404; res.set_content("<h1>404 Not Found</h1>", "text/html"); return; }
        std::string tmpl = load_tmpl("admin_problem_form.html");
        std::map<std::string, std::string> vars = {
            {"TITLE", "Edit Problem #" + std::to_string(id)},
            {"TITLE_VALUE", p.title},
            {"DIFFICULTY_OPTIONS", build_difficulty_options(p.difficulty)},
            {"CONTENT", p.content},
            {"TEMPLATE", p.template_code},
            {"SUBMIT_FN", "updateProblem"},
            {"SUBMIT_LABEL", "Save"},
            {"ID", std::to_string(id)},
        };
        std::string body = replace_all(tmpl, vars);
        res.set_content(render_page("Edit Problem", body, nav_user(session->username, true)), "text/html");
    });

    svr.Post(R"(/admin/problems/(\d+)/edit)", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH_JSON(req, res);
        CHECK_ADMIN_JSON(res);
        try {
            int id = std::stoi(req.matches[1]);
            json body = json::parse(req.body);
            std::string title = body.value("title", "");
            std::string difficulty = body.value("difficulty", "");
            std::string content = body.value("content", "");
            std::string template_code = body.value("template", "");
            if (title.empty() || difficulty.empty() || content.empty()) {
                json resp = {{"success", false}, {"error", "Title, difficulty, and content are required"}};
                res.status = 400;
                res.set_content(resp.dump(), "application/json");
                return;
            }
            db.update_problem(id, title, difficulty, content, template_code);
            LOG_INFO("problem updated: id=" + std::to_string(id) + " title=" + title);
            json resp = {{"success", true}, {"redirect", "/admin"}};
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json resp = {{"success", false}, {"error", std::string("Bad request: ") + e.what()}};
            res.set_content(resp.dump(), "application/json");
        }
    });

    svr.Post(R"(/admin/problems/(\d+)/delete)", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH_JSON(req, res);
        CHECK_ADMIN_JSON(res);
        int id = std::stoi(req.matches[1]);
        Problem p = db.get_problem(id);
        if (p.id == 0) {
            json resp = {{"success", false}, {"error", "Problem not found"}};
            res.status = 404;
            res.set_content(resp.dump(), "application/json");
            return;
        }
        db.delete_problem(id);
        LOG_INFO("problem deleted: id=" + std::to_string(id) + " title=" + p.title);
        json resp = {{"success", true}, {"redirect", "/admin"}};
        res.set_content(resp.dump(), "application/json");
    });

    svr.Get(R"(/admin/problems/(\d+)/testcases)", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        CHECK_ADMIN(res);
        int problem_id = std::stoi(req.matches[1]);
        Problem p = db.get_problem(problem_id);
        if (p.id == 0) { res.status = 404; res.set_content("<h1>404 Not Found</h1>", "text/html"); return; }
        auto cases = db.get_test_cases(problem_id);
        std::string tmpl = load_tmpl("admin_testcases.html");
        std::map<std::string, std::string> vars = {
            {"PROBLEM_ID", std::to_string(problem_id)},
            {"PROBLEM_TITLE", "Problem #" + std::to_string(problem_id) + " - " + p.title},
            {"TESTCASE_ROWS", build_tc_rows(cases, problem_id)},
        };
        std::string body = replace_all(tmpl, vars);
        res.set_content(render_page("Test Cases", body, nav_user(session->username, true)), "text/html");
    });

    svr.Post(R"(/admin/problems/(\d+)/testcases)", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH_JSON(req, res);
        CHECK_ADMIN_JSON(res);
        try {
            int problem_id = std::stoi(req.matches[1]);
            json body = json::parse(req.body);
            std::string input = body.value("input", "");
            std::string expected = body.value("expected", "");
            int position = body.value("position", 0);
            if (input.empty() || expected.empty()) {
                json resp = {{"success", false}, {"error", "Input and expected output are required"}};
                res.status = 400;
                res.set_content(resp.dump(), "application/json");
                return;
            }
            int id = db.insert_test_case(problem_id, input, expected, position);
            LOG_INFO("test case added: problem_id=" + std::to_string(problem_id));
            json resp = {{"success", true}, {"id", id}};
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json resp = {{"success", false}, {"error", std::string("Bad request: ") + e.what()}};
            res.set_content(resp.dump(), "application/json");
        }
    });

    svr.Post(R"(/admin/testcases/(\d+)/delete)", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH_JSON(req, res);
        CHECK_ADMIN_JSON(res);
        try {
            int case_id = std::stoi(req.matches[1]);
            json body = json::parse(req.body);
            int problem_id = body.value("problem_id", 0);
            (void)problem_id;
            db.delete_test_case(case_id);
            LOG_INFO("test case deleted: id=" + std::to_string(case_id));
            json resp = {{"success", true}};
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json resp = {{"success", false}, {"error", std::string("Bad request: ") + e.what()}};
            res.set_content(resp.dump(), "application/json");
        }
    });

    svr.Get("/admin/users", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        CHECK_ADMIN(res);
        auto users = db.get_all_users();
        std::string tmpl = load_tmpl("admin_users.html");
        std::map<std::string, std::string> vars = {
            {"USER_ROWS", build_user_rows(users)},
        };
        std::string body = replace_all(tmpl, vars);
        res.set_content(render_page("Users", body, nav_user(session->username, true)), "text/html");
    });

    svr.listen("0.0.0.0", 8080);
    return 0;
}
