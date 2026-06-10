#include "deps/cpp-httplib/httplib.h"
#include "config.h"
#include "db.h"
#include "render.h"
#include "auth.h"
#include "judge.h"
#include "md.h"
#include "log.h"
#include <iostream>

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
        std::string tmpl = read_template("templates/login.html");
        if (tmpl.empty()) {
            tmpl = "<h1>Login</h1>"
                   "<form method=\"POST\" action=\"/login\">"
                   "<input name=\"username\" placeholder=\"Username\"><br>"
                   "<input type=\"password\" name=\"password\" placeholder=\"Password\"><br>"
                   "<button type=\"submit\">Login</button>"
                   "</form>";
        }
        tmpl = replace(tmpl, "ERROR", "");
        res.set_content(render_page("Login", tmpl, nav_public()), "text/html");
    });

    svr.Post("/login", [](const httplib::Request& req, httplib::Response& res) {
        std::string username = req.get_param_value("username");
        std::string password = req.get_param_value("password");
        User user = db.get_user_by_username(username);
        if (user.id == 0 || !bcrypt_verify(password, user.password)) {
            LOG_WARN("login failed: username=" + username);
            std::string tmpl = read_template("templates/login.html");
            if (tmpl.empty()) {
                tmpl = "<h1>Login</h1>"
                       "<p style=\"color:red\">Invalid username or password</p>"
                       "<form method=\"POST\" action=\"/login\">"
                       "<input name=\"username\" placeholder=\"Username\"><br>"
                       "<input type=\"password\" name=\"password\" placeholder=\"Password\"><br>"
                       "<button type=\"submit\">Login</button>"
                       "</form>";
            } else {
                tmpl = replace(tmpl, "ERROR", "<p style=\"color:red\">Invalid username or password</p>");
            }
            res.set_content(render_page("Login", tmpl, nav_public()), "text/html");
            return;
        }
        std::string token = create_session(user.id, user.username, user.role);
        LOG_INFO("login success: username=" + user.username);
        res.set_header("Set-Cookie", "session_id=" + token + "; Path=/; HttpOnly");
        res.set_redirect("/problems");
    });

    svr.Get("/register", [](const httplib::Request&, httplib::Response& res) {
        std::string tmpl = read_template("templates/register.html");
        if (tmpl.empty()) {
            tmpl = "<h1>Register</h1>"
                   "<form method=\"POST\" action=\"/register\">"
                   "<input name=\"username\" placeholder=\"Username\"><br>"
                   "<input type=\"password\" name=\"password\" placeholder=\"Password\"><br>"
                   "<button type=\"submit\">Register</button>"
                   "</form>";
        }
        tmpl = replace(tmpl, "ERROR", "");
        res.set_content(render_page("Register", tmpl, nav_public()), "text/html");
    });

    svr.Post("/register", [](const httplib::Request& req, httplib::Response& res) {
        std::string username = req.get_param_value("username");
        std::string password = req.get_param_value("password");
        if (username.empty() || password.empty()) {
            std::string tmpl = read_template("templates/register.html");
            if (tmpl.empty()) {
                tmpl = "<h1>Register</h1>"
                       "<p style=\"color:red\">All fields required</p>"
                       "<form method=\"POST\" action=\"/register\">"
                       "<input name=\"username\" placeholder=\"Username\"><br>"
                       "<input type=\"password\" name=\"password\" placeholder=\"Password\"><br>"
                       "<button type=\"submit\">Register</button>"
                       "</form>";
            } else {
                tmpl = replace(tmpl, "ERROR", "<p style=\"color:red\">All fields required</p>");
            }
            res.set_content(render_page("Register", tmpl, nav_public()), "text/html");
            return;
        }
        User existing = db.get_user_by_username(username);
        if (existing.id != 0) {
            std::string tmpl = read_template("templates/register.html");
            if (tmpl.empty()) {
                tmpl = "<h1>Register</h1>"
                       "<p style=\"color:red\">Username already taken</p>"
                       "<form method=\"POST\" action=\"/register\">"
                       "<input name=\"username\" placeholder=\"Username\"><br>"
                       "<input type=\"password\" name=\"password\" placeholder=\"Password\"><br>"
                       "<button type=\"submit\">Register</button>"
                       "</form>";
            } else {
                tmpl = replace(tmpl, "ERROR", "<p style=\"color:red\">Username already taken</p>");
            }
            res.set_content(render_page("Register", tmpl, nav_public()), "text/html");
            return;
        }
        std::string hash = bcrypt_hash(password);
        db.insert_user(username, hash);
        LOG_INFO("user registered: username=" + username);
        res.set_redirect("/login");
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
        std::string template_code = p.template_code;
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
            "<form method=\"POST\" action=\"/problem/" + std::to_string(id) + "/submit\">"
            "<textarea name=\"code\" rows=\"20\" cols=\"60\" style=\"width:100%\">" + template_code + "</textarea><br><br>"
            "<button type=\"submit\">Submit</button>"
            "</form>"
            "<div id=\"result\"></div>"
            "</div>"
            "</div>";
        res.set_content(render_page(p.title, body, nav_user(session->username, session->role == "admin")), "text/html");
    });

    svr.Post(R"(/problem/(\d+)/submit)", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        int id = std::stoi(req.matches[1]);
        Problem p = db.get_problem(id);
        if (p.id == 0) { res.status = 404; res.set_content("<h1>404 Not Found</h1>", "text/html"); return; }
        std::string code = req.get_param_value("code");
        if (code.empty()) {
            res.status = 400;
            res.set_content("<h1>400 Bad Request</h1><p>Code is required.</p>", "text/html");
            return;
        }
        auto cases = db.get_test_cases(id);
        std::vector<JudgeCase> judge_cases;
        for (const auto& tc : cases) {
            JudgeCase jc;
            jc.input = tc.input;
            jc.expected = tc.expected;
            judge_cases.push_back(jc);
        }
        JudgeResult jr = compile_and_judge(code, judge_cases);
        std::string result_html = "<h3>Result</h3>";
        std::string color;
        if (jr.status == "AC") color = "green";
        else if (jr.status == "CE") color = "#c0a000";
        else color = "red";
        result_html += "<p style=\"color:" + color + "; font-size:1.2em; font-weight:bold\">" + jr.status + "</p>";
        if (jr.status == "CE") {
            result_html += "<pre>" + jr.compile_error + "</pre>";
        } else if (jr.status == "WA") {
            result_html += "<p>Failed on test case #" + std::to_string(jr.failed_case) + "</p>";
            result_html += "<p><strong>Expected:</strong></p><pre>" + jr.expected_output + "</pre>";
            result_html += "<p><strong>Actual:</strong></p><pre>" + jr.actual_output + "</pre>";
        }
        if (jr.time_ms > 0) {
            result_html += "<p>Time: " + std::to_string(jr.time_ms) + "ms</p>";
        }
        std::string desc_html = md_to_html(p.content);
        auto cases_list = db.get_test_cases(id);
        std::string sample_html;
        if (!cases_list.empty()) {
            sample_html += "<h3>Sample Test Cases</h3>";
            for (size_t i = 0; i < cases_list.size(); i++) {
                sample_html += "<p><strong>Input:</strong></p><pre>" + cases_list[i].input + "</pre>"
                               "<p><strong>Expected:</strong></p><pre>" + cases_list[i].expected + "</pre>";
                if (i < cases_list.size() - 1) sample_html += "<hr>";
            }
        }
        std::string template_code = p.template_code;
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
            "<form method=\"POST\" action=\"/problem/" + std::to_string(id) + "/submit\">"
            "<textarea name=\"code\" rows=\"20\" cols=\"60\" style=\"width:100%\">" + code + "</textarea><br><br>"
            "<button type=\"submit\">Submit</button>"
            "</form>"
            + result_html +
            "</div>"
            "</div>";
        res.set_content(render_page(p.title + " - Result", body, nav_user(session->username, session->role == "admin")), "text/html");
    });

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
                    "<form method=\"POST\" action=\"/admin/problems/" + std::to_string(p.id) + "/delete\" style=\"display:inline\">"
                    "<button type=\"submit\" onclick=\"return confirm('Delete this problem?')\">Delete</button>"
                    "</form>"
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
                           "<form method=\"POST\" action=\"/admin/problems\">"
                           "<label>Title:</label><br><input name=\"title\" required><br><br>"
                           "<label>Difficulty:</label><br>"
                           "<select name=\"difficulty\">"
                           "<option>Easy</option><option>Medium</option><option>Hard</option>"
                           "</select><br><br>"
                           "<label>Description (Markdown):</label><br>"
                           "<textarea name=\"content\" rows=\"10\" cols=\"60\"></textarea><br><br>"
                           "<label>Code Template (optional):</label><br>"
                           "<textarea name=\"template\" rows=\"6\" cols=\"60\"></textarea><br><br>"
                           "<button type=\"submit\">Create</button>"
                           "</form>"
                           "<p><a href=\"/admin\">Back</a></p>";
        res.set_content(render_page("New Problem", body, nav_user(session->username, true)), "text/html");
    });

    svr.Post("/admin/problems", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        CHECK_ADMIN(res);
        std::string title = req.get_param_value("title");
        std::string difficulty = req.get_param_value("difficulty");
        std::string content = req.get_param_value("content");
        std::string template_code = req.get_param_value("template");
        if (title.empty() || difficulty.empty() || content.empty()) {
            res.status = 400;
            res.set_content("<h1>400 Bad Request</h1><p>Title, difficulty, and content are required.</p>", "text/html");
            return;
        }
        int id = db.insert_problem(title, difficulty, content, template_code);
        LOG_INFO("problem created: id=" + std::to_string(id) + " title=" + title);
        res.set_redirect("/admin");
    });

    svr.Get(R"(/admin/problems/(\d+)/edit)", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        CHECK_ADMIN(res);
        int id = std::stoi(req.matches[1]);
        Problem p = db.get_problem(id);
        if (p.id == 0) { res.status = 404; res.set_content("<h1>404 Not Found</h1>", "text/html"); return; }
        auto selected = [&](const std::string& d) { return p.difficulty == d ? " selected" : ""; };
        std::string body = "<h1>Edit Problem #" + std::to_string(id) + "</h1>"
                           "<form method=\"POST\" action=\"/admin/problems/" + std::to_string(id) + "/edit\">"
                           "<label>Title:</label><br><input name=\"title\" value=\"" + p.title + "\" required><br><br>"
                           "<label>Difficulty:</label><br>"
                           "<select name=\"difficulty\">"
                           "<option" + selected("Easy") + ">Easy</option>"
                           "<option" + selected("Medium") + ">Medium</option>"
                           "<option" + selected("Hard") + ">Hard</option>"
                           "</select><br><br>"
                           "<label>Description (Markdown):</label><br>"
                           "<textarea name=\"content\" rows=\"10\" cols=\"60\">" + p.content + "</textarea><br><br>"
                           "<label>Code Template (optional):</label><br>"
                           "<textarea name=\"template\" rows=\"6\" cols=\"60\">" + p.template_code + "</textarea><br><br>"
                           "<button type=\"submit\">Save</button>"
                           "</form>"
                           "<p><a href=\"/admin\">Back</a></p>";
        res.set_content(render_page("Edit Problem", body, nav_user(session->username, true)), "text/html");
    });

    svr.Post(R"(/admin/problems/(\d+)/edit)", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        CHECK_ADMIN(res);
        int id = std::stoi(req.matches[1]);
        std::string title = req.get_param_value("title");
        std::string difficulty = req.get_param_value("difficulty");
        std::string content = req.get_param_value("content");
        std::string template_code = req.get_param_value("template");
        if (title.empty() || difficulty.empty() || content.empty()) {
            res.status = 400;
            res.set_content("<h1>400 Bad Request</h1><p>Title, difficulty, and content are required.</p>", "text/html");
            return;
        }
        db.update_problem(id, title, difficulty, content, template_code);
        LOG_INFO("problem updated: id=" + std::to_string(id) + " title=" + title);
        res.set_redirect("/admin");
    });

    svr.Post(R"(/admin/problems/(\d+)/delete)", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        CHECK_ADMIN(res);
        int id = std::stoi(req.matches[1]);
        Problem p = db.get_problem(id);
        if (p.id == 0) { res.status = 404; res.set_content("<h1>404 Not Found</h1>", "text/html"); return; }
        db.delete_problem(id);
        LOG_INFO("problem deleted: id=" + std::to_string(id) + " title=" + p.title);
        res.set_redirect("/admin");
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
                    "<form method=\"POST\" action=\"/admin/testcases/" + std::to_string(tc.id) + "/delete\" style=\"display:inline\">"
                    "<input type=\"hidden\" name=\"problem_id\" value=\"" + std::to_string(problem_id) + "\">"
                    "<button type=\"submit\" onclick=\"return confirm('Delete this test case?')\">Delete</button>"
                    "</form>"
                    "</td></tr>";
        }
        std::string body = "<h1>Test Cases for Problem #" + std::to_string(problem_id) + " - " + p.title + "</h1>"
                           "<p><a href=\"/admin\">Back to Admin</a></p>"
                           "<h2>Add Test Case</h2>"
                           "<form method=\"POST\" action=\"/admin/problems/" + std::to_string(problem_id) + "/testcases\">"
                           "<label>Input (stdin):</label><br>"
                           "<textarea name=\"input\" rows=\"4\" cols=\"50\"></textarea><br><br>"
                           "<label>Expected Output (stdout):</label><br>"
                           "<textarea name=\"expected\" rows=\"4\" cols=\"50\"></textarea><br><br>"
                           "<label>Position:</label><br>"
                           "<input name=\"position\" type=\"number\" value=\"0\"><br><br>"
                           "<button type=\"submit\">Add Test Case</button>"
                           "</form>"
                           "<h2>Test Cases</h2>"
                           "<table border=\"1\"><tr><th>ID</th><th>Pos</th><th>Input</th><th>Expected</th><th>Action</th></tr>" +
                           (rows.empty() ? "<tr><td colspan=\"5\">No test cases yet</td></tr>" : rows) +
                           "</table>";
        res.set_content(render_page("Test Cases", body, nav_user(session->username, true)), "text/html");
    });

    svr.Post(R"(/admin/problems/(\d+)/testcases)", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        CHECK_ADMIN(res);
        int problem_id = std::stoi(req.matches[1]);
        std::string input = req.get_param_value("input");
        std::string expected = req.get_param_value("expected");
        std::string pos_str = req.get_param_value("position");
        int position = pos_str.empty() ? 0 : std::stoi(pos_str);
        if (input.empty() || expected.empty()) {
            res.status = 400;
            res.set_content("<h1>400 Bad Request</h1><p>Input and expected output are required.</p>", "text/html");
            return;
        }
        db.insert_test_case(problem_id, input, expected, position);
        LOG_INFO("test case added: problem_id=" + std::to_string(problem_id));
        res.set_redirect("/admin/problems/" + std::to_string(problem_id) + "/testcases");
    });

    svr.Post(R"(/admin/testcases/(\d+)/delete)", [](const httplib::Request& req, httplib::Response& res) {
        CHECK_AUTH(req, res);
        CHECK_ADMIN(res);
        int case_id = std::stoi(req.matches[1]);
        std::string problem_id = req.get_param_value("problem_id");
        db.delete_test_case(case_id);
        LOG_INFO("test case deleted: id=" + std::to_string(case_id));
        res.set_redirect("/admin/problems/" + problem_id + "/testcases");
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
