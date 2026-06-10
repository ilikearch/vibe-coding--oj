#pragma once

#include <string>
#include <unordered_map>
#include <ctime>

std::string bcrypt_hash(const std::string& pass);
bool bcrypt_verify(const std::string& pass, const std::string& hash);

struct Session {
    int user_id;
    std::string username;
    std::string role;
    time_t created_at;
};

std::string generate_session_token();
Session* get_session(const std::string& token);
std::string create_session(int user_id, const std::string& username, const std::string& role);
void destroy_session(const std::string& token);

std::string get_cookie(const std::string& cookie_header, const std::string& key);

#define CHECK_AUTH(req, res) \
    Session* session = nullptr; \
    do { \
        std::string sid = get_cookie(req.get_header_value("Cookie"), "session_id"); \
        session = get_session(sid); \
        if (!session) { res.set_redirect("/login"); return; } \
    } while(0)

#define CHECK_ADMIN(res) \
    do { \
        if (!session || session->role != "admin") { \
            res.status = 403; \
            res.set_content("<h1>403 Forbidden</h1>", "text/html"); \
            return; \
        } \
    } while(0)

#define CHECK_AUTH_JSON(req, res) \
    Session* session = nullptr; \
    do { \
        std::string sid = get_cookie(req.get_header_value("Cookie"), "session_id"); \
        session = get_session(sid); \
        if (!session) { \
            res.status = 401; \
            res.set_content("{\"success\":false,\"error\":\"Unauthorized\"}", "application/json"); \
            return; \
        } \
    } while(0)

#define CHECK_ADMIN_JSON(res) \
    do { \
        if (!session || session->role != "admin") { \
            res.status = 403; \
            res.set_content("{\"success\":false,\"error\":\"Forbidden\"}", "application/json"); \
            return; \
        } \
    } while(0)
