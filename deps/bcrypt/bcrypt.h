#pragma once
#include <crypt.h>
#include <string>

inline std::string bcrypt_hash(const std::string& pw) {
    char* salt = crypt_gensalt("$2b$", 10, nullptr, 0);
    char* h = crypt(pw.c_str(), salt);
    return h ? std::string(h) : "";
}
inline bool bcrypt_verify(const std::string& pw, const std::string& hash) {
    char* r = crypt(pw.c_str(), hash.c_str());
    return r != nullptr && hash == r;
}
