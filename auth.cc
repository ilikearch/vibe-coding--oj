#include "auth.h"
#include "log.h"
#include <crypt.h>
#include <random>
#include <cstring>
#include <sstream>
#include <thread>
#include <mutex>

static const char* BCRYPT_BASE64 = "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

static std::string gen_rand_str(size_t len, const char* alphabet, size_t alpha_len) {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, alpha_len - 1);
    std::string s;
    s.reserve(len);
    for (size_t i = 0; i < len; i++) s += alphabet[dist(rng)];
    return s;
}

std::string bcrypt_hash(const std::string& pass) {
    std::string salt = "$2a$10$" + gen_rand_str(22, BCRYPT_BASE64, 64);
    char* result = crypt(pass.c_str(), salt.c_str());
    if (!result) return "";
    return std::string(result);
}

bool bcrypt_verify(const std::string& pass, const std::string& hash) {
    char* result = crypt(pass.c_str(), hash.c_str());
    if (!result) return false;
    return std::string(result) == hash;
}

static std::unordered_map<std::string, Session> sessions;
static std::mutex sessions_mutex;

std::string generate_session_token() {
    return gen_rand_str(32, "0123456789abcdef", 16);
}

Session* get_session(const std::string& token) {
    std::lock_guard<std::mutex> lock(sessions_mutex);
    auto it = sessions.find(token);
    if (it == sessions.end()) return nullptr;
    return &it->second;
}

std::string create_session(int user_id, const std::string& username, const std::string& role) {
    std::lock_guard<std::mutex> lock(sessions_mutex);
    Session s;
    s.user_id = user_id;
    s.username = username;
    s.role = role;
    s.created_at = time(nullptr);
    std::string token = generate_session_token();
    sessions[token] = s;
    LOG_INFO("session created: user=" + username + " role=" + role);
    return token;
}

void destroy_session(const std::string& token) {
    std::lock_guard<std::mutex> lock(sessions_mutex);
    sessions.erase(token);
}

std::string get_cookie(const std::string& cookie_header, const std::string& key) {
    if (cookie_header.empty()) return "";
    size_t pos = 0;
    while (pos < cookie_header.size()) {
        while (pos < cookie_header.size() && cookie_header[pos] == ' ') pos++;
        size_t eq = cookie_header.find('=', pos);
        if (eq == std::string::npos) break;
        std::string k = cookie_header.substr(pos, eq - pos);
        size_t end = cookie_header.find(';', eq + 1);
        if (end == std::string::npos) end = cookie_header.size();
        std::string v = cookie_header.substr(eq + 1, end - eq - 1);
        if (k == key) return v;
        pos = end + 1;
    }
    return "";
}
