#pragma once

#include <string>
#include <map>

std::string read_template(const std::string& path);

std::string replace(const std::string& tmpl, const std::string& key, const std::string& value);

std::string replace_all(const std::string& tmpl, const std::map<std::string, std::string>& vars);

std::string render_page(const std::string& title, const std::string& body,
                        const std::string& nav_html = "");
