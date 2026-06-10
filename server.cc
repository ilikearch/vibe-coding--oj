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

int main() {
    LOG_INFO("Vibe OJ Server starting on port 8080...");
    std::cout << "Vibe OJ Server starting on port 8080..." << std::endl;
    httplib::Server svr;

    svr.set_mount_point("/", "./static");

    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        std::string sid = get_cookie(req.get_header_value("Cookie"), "session_id");
        Session* s = get_session(sid);
        std::string nav = s ? nav_user(s->username, s->role == "admin") : nav_public();
        int pcount = db.count_problems();
        int ucount = db.count_users();
        std::string body =
            "<h1>Vibe OJ</h1>"
            "<p>A lightweight C++ Online Judge for small teams (3-20 people).</p>"
            "<h2>Features</h2>"
            "<ul>"
            "<li>C++ (g++) code judging with stdin/stdout comparison</li>"
            "<li>Sandboxed execution (rlimit + seccomp + chroot)</li>"
            "<li>Simple signup/login with bcrypt password hashing</li>"
            "<li>Admin panel for problem &amp; test case management</li>"
            "</ul>"
            "<h2>Stats</h2>"
            "<p>Problems: " + std::to_string(pcount) + " | Users: " + std::to_string(ucount) + "</p>"
            "<p>"
            "<a href=\"/problems\"><b>Browse Problems</b></a> | "
            "<a href=\"/login\">Login</a> | "
            "<a href=\"/register\">Register</a>"
            "</p>";
        res.set_content(render_page("Home", body, nav), "text/html");
    });

    svr.Get("/login", [](const httplib::Request&, httplib::Response& res) {
        std::string body =
            "<h1>Login</h1>"
            "<div id=\"login-error\" style=\"color:red\"></div>"
            "<input id=\"login-username\" placeholder=\"Username\"><br><br>"
            "<input id=\"login-password\" type=\"password\" placeholder=\"Password\"><br><br>"
            "<button onclick=\"doLogin()\">Login</button>";
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
        std::string body =
            "<h1>Register</h1>"
            "<div id=\"register-error\" style=\"color:red\"></div>"
            "<input id=\"register-username\" placeholder=\"Username\"><br><br>"
            "<input id=\"register-password\" type=\"password\" placeholder=\"Password\"><br><br>"
            "<button onclick=\"doRegister()\">Register</button>";
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

    svr.Get("/problems", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        auto problems = db.get_all_problems();
        std::string rows;
        for (const auto& p : problems) {
            rows += "<tr>"
                    "<td>" + std::to_string(p.id) + "</td>"
                    "<td><a href=\"/problem/" + std::to_string(p.id) + "\">" + p.title + "</a></td>"
                    "<td>" + p.difficulty + "</td>"
                    "</tr>";
        }
        std::string body = "<h1>Problems</h1>"
                           "<table border=\"1\"><tr><th>#</th><th>Title</th><th>Difficulty</th></tr>" +
                           (rows.empty() ? "<tr><td colspan=\"3\">No problems yet</td></tr>" : rows) +
                           "</table>";
        res.set_content(render_page("Problems", body, nav_user(session->username, session->role == "admin")), "text/html");
    });

    svr.Get(R"(/problem/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        int id = std::stoi(req.matches[1]);
        Problem p = db.get_problem(id);
        if (p.id == 0) { res.status = 404; res.set_content("<h1>404 Not Found</h1>", "text/html"); return; }
        std::string desc_html = md_to_html(p.content);
        auto cases = db.get_test_cases(id);
        std::string sample_html;
        if (!cases.empty()) {
            sample_html += "<h3>Sample Test Cases</h3>";
            for (size_t i = 0; i < cases.size(); i++) {
                sample_html += "<p><strong>Input:</strong></p><pre>" + cases[i].input + "</pre>"
                               "<p><strong>Expected:</strong></p><pre>" + cases[i].expected + "</pre>";
                if (i < cases.size() - 1) sample_html += "<hr>";
            }
        }
        std::string body =
            "<div style=\"display:flex; gap:40px;\">"
            "<div style=\"flex:1;\">"
            "<h1>" + p.title + "</h1>"
            "<span class=\"difficulty " + p.difficulty + "\">" + p.difficulty + "</span>"
            "<div>" + desc_html + "</div>"
            + sample_html +
            "</div>"
            "<div style=\"flex:1;\">"
            "<h2>Submit Solution</h2>"
            "<textarea id=\"code-area\" rows=\"20\" style=\"width:100%\">" + p.template_code + "</textarea><br><br>"
            "<button onclick=\"submitCode(" + std::to_string(id) + ")\">Submit</button>"
            "<div id=\"submit-result\"></div>"
            "</div>"
            "</div>";
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
        std::string rows;
        for (const auto& p : problems) {
            rows += "<tr>"
                    "<td>" + std::to_string(p.id) + "</td>"
                    "<td>" + p.title + "</td>"
                    "<td>" + p.difficulty + "</td>"
                    "<td>"
                    "<a href=\"/admin/problems/" + std::to_string(p.id) + "/edit\">Edit</a> | "
                    "<a href=\"/admin/problems/" + std::to_string(p.id) + "/testcases\">Test Cases</a> | "
                    "<button onclick=\"deleteProblem(" + std::to_string(p.id) + ")\">Delete</button>"
                    "</td></tr>";
        }
        std::string body = "<h1>Admin Panel</h1>"
                           "<p><a href=\"/admin/problems/new\">+ New Problem</a> | <a href=\"/admin/users\">Users</a></p>"
                           "<table border=\"1\"><tr><th>ID</th><th>Title</th><th>Difficulty</th><th>Actions</th></tr>" +
                           (rows.empty() ? "<tr><td colspan=\"4\">No problems yet</td></tr>" : rows) +
                           "</table>";
        res.set_content(render_page("Admin Panel", body, nav_user(session->username, true)), "text/html");
    });

    svr.Get("/admin/problems/new", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        CHECK_ADMIN(res);
        std::string body = "<h1>New Problem</h1>"
                           "<label>Title:</label><br><input id=\"prob-title\" required><br><br>"
                           "<label>Difficulty:</label><br>"
                           "<select id=\"prob-difficulty\">"
                           "<option>Easy</option><option>Medium</option><option>Hard</option>"
                           "</select><br><br>"
                           "<label>Description (Markdown):</label><br>"
                           "<textarea id=\"prob-content\" rows=\"10\" cols=\"60\"></textarea><br><br>"
                           "<label>Code Template (optional):</label><br>"
                           "<textarea id=\"prob-template\" rows=\"6\" cols=\"60\"></textarea><br><br>"
                           "<button onclick=\"createProblem()\">Create</button>"
                           "<div id=\"form-result\"></div>"
                           "<p><a href=\"/admin\">Back</a></p>";
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
        auto selected = [&](const std::string& d) { return p.difficulty == d ? " selected" : ""; };
        std::string body = "<h1>Edit Problem #" + std::to_string(id) + "</h1>"
                           "<label>Title:</label><br><input id=\"prob-title\" value=\"" + p.title + "\" required><br><br>"
                           "<label>Difficulty:</label><br>"
                           "<select id=\"prob-difficulty\">"
                           "<option" + selected("Easy") + ">Easy</option>"
                           "<option" + selected("Medium") + ">Medium</option>"
                           "<option" + selected("Hard") + ">Hard</option>"
                           "</select><br><br>"
                           "<label>Description (Markdown):</label><br>"
                           "<textarea id=\"prob-content\" rows=\"10\" cols=\"60\">" + p.content + "</textarea><br><br>"
                           "<label>Code Template (optional):</label><br>"
                           "<textarea id=\"prob-template\" rows=\"6\" cols=\"60\">" + p.template_code + "</textarea><br><br>"
                           "<button onclick=\"updateProblem(" + std::to_string(id) + ")\">Save</button>"
                           "<div id=\"form-result\"></div>"
                           "<p><a href=\"/admin\">Back</a></p>";
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
        std::string rows;
        for (const auto& tc : cases) {
            rows += "<tr>"
                    "<td>" + std::to_string(tc.id) + "</td>"
                    "<td>" + std::to_string(tc.position) + "</td>"
                    "<td><pre>" + tc.input + "</pre></td>"
                    "<td><pre>" + tc.expected + "</pre></td>"
                    "<td>"
                    "<button onclick=\"deleteTestCase(" + std::to_string(tc.id) + "," + std::to_string(problem_id) + ")\">Delete</button>"
                    "</td></tr>";
        }
        std::string body = "<h1>Test Cases for Problem #" + std::to_string(problem_id) + " - " + p.title + "</h1>"
                           "<p><a href=\"/admin\">Back to Admin</a></p>"
                           "<h2>Add Test Case</h2>"
                           "<label>Input (stdin):</label><br>"
                           "<textarea id=\"tc-input\" rows=\"4\" cols=\"50\"></textarea><br><br>"
                           "<label>Expected Output (stdout):</label><br>"
                           "<textarea id=\"tc-expected\" rows=\"4\" cols=\"50\"></textarea><br><br>"
                           "<label>Position:</label><br>"
                           "<input id=\"tc-position\" type=\"number\" value=\"0\"><br><br>"
                           "<button onclick=\"addTestCase(" + std::to_string(problem_id) + ")\">Add Test Case</button>"
                           "<div id=\"tc-result\"></div>"
                           "<h2>Test Cases</h2>"
                           "<table border=\"1\"><tr><th>ID</th><th>Pos</th><th>Input</th><th>Expected</th><th>Action</th></tr>" +
                           (rows.empty() ? "<tr><td colspan=\"5\">No test cases yet</td></tr>" : rows) +
                           "</table>";
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
        std::string rows;
        for (const auto& u : users) {
            rows += "<tr>"
                    "<td>" + std::to_string(u.id) + "</td>"
                    "<td>" + u.username + "</td>"
                    "<td>" + u.role + "</td>"
                    "<td>" + u.created_at + "</td>"
                    "</tr>";
        }
        std::string body = "<h1>Users</h1>"
                           "<p><a href=\"/admin\">Back to Admin</a></p>"
                           "<table border=\"1\"><tr><th>ID</th><th>Username</th><th>Role</th><th>Created</th></tr>" +
                           (rows.empty() ? "<tr><td colspan=\"4\">No users</td></tr>" : rows) +
                           "</table>";
        res.set_content(render_page("Users", body, nav_user(session->username, true)), "text/html");
    });
    svr.listen("0.0.0.0", 8080);
    return 0;
}