#include <string>

std::string bcrypt_hash(const std::string& pass);
bool bcrypt_verify(const std::string& pass, const std::string& hash);
