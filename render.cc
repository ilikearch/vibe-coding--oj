#include "render.h"
#include <fstream>
#include <sstream>

std::string read_template(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string replace(const std::string& tmpl, const std::string& key, const std::string& value) {
    std::string needle = "{{" + key + "}}";
    std::string result = tmpl;
    size_t pos = 0;
    while ((pos = result.find(needle, pos)) != std::string::npos) {
        result.replace(pos, needle.size(), value);
        pos += value.size();
    }
    return result;
}

std::string replace_all(const std::string& tmpl, const std::map<std::string, std::string>& vars) {
    std::string result = tmpl;
    for (const auto& [key, value] : vars) {
        result = replace(result, key, value);
    }
    return result;
}

std::string render_page(const std::string& title, const std::string& body,
                        const std::string& nav_html) {
    std::string tmpl = read_template("templates/_base.html");
    if (tmpl.empty()) {
        std::ostringstream ss;
        ss << "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n"
           << "<title>" << title << " - Vibe OJ</title>\n"
           << "<link rel=\"stylesheet\" href=\"/style.css\">\n</head>\n<body>\n"
           << "<nav>" << nav_html << "</nav>\n"
           << "<main>\n" << body << "\n</main>\n"
           << "<script src=\"/app.js\"></script>\n"
           << "</body>\n</html>";
        return ss.str();
    }
    std::map<std::string, std::string> vars = {
        {"TITLE", title},
        {"BODY", body},
        {"NAV", nav_html},
    };
    return replace_all(tmpl, vars);
}
