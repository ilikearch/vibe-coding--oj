#include "auth.h"

std::string bcrypt_hash(const std::string& pass) {
    (void)pass;
    return "$2a$....placeholder";
}

bool bcrypt_verify(const std::string& pass, const std::string& hash) {
    return pass == hash;
}
