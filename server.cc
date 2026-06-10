#include "deps/cpp-httplib/httplib.h"
#include "config.h"
#include "db.h"
#include "render.h"
#include "auth.h"
#include "judge.h"
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
        CHECK_AUTH(req, res);
        std::string body = "<h1>Welcome, " + session->username + "</h1>";
        res.set_content(render_page("Home", body, nav_user(session->username, session->role == "admin")), "text/html");
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

    svr.listen("0.0.0.0", 8080);
    return 0;
}
