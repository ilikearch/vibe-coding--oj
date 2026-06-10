#include "md.h"
#include <sstream>

std::string md_to_html(const std::string& markdown) {
    std::string html;
    std::istringstream stream(markdown);
    std::string line;
    bool in_code_block = false;
    bool in_list = false;

    while (std::getline(stream, line)) {
        if (line.empty() || line == "\r") {
            if (in_list) { html += "</ul>\n"; in_list = false; }
            continue;
        }

        if (line.size() >= 3 && line.substr(0, 3) == "```") {
            if (!in_code_block) {
                if (in_list) { html += "</ul>\n"; in_list = false; }
                html += "<pre><code>";
                in_code_block = true;
            } else {
                html += "</code></pre>\n";
                in_code_block = false;
            }
            continue;
        }

        if (in_code_block) {
            for (size_t i = 0; i < line.size(); i++) {
                switch (line[i]) {
                    case '<': html += "&lt;"; break;
                    case '>': html += "&gt;"; break;
                    case '&': html += "&amp;"; break;
                    default: html += line[i];
                }
            }
            html += "\n";
            continue;
        }

        if (line.size() >= 2 && line[0] == '-' && line[1] == ' ') {
            if (!in_list) { html += "<ul>\n"; in_list = true; }
            html += "<li>" + line.substr(2) + "</li>\n";
            continue;
        }

        if (in_list) { html += "</ul>\n"; in_list = false; }

        int h_level = 0;
        while (h_level < 6 && h_level < (int)line.size() && line[h_level] == '#') h_level++;
        if (h_level > 0 && h_level < (int)line.size() && line[h_level] == ' ') {
            std::string heading = line.substr(h_level + 1);
            html += "<h" + std::to_string(h_level) + ">" + heading + "</h" + std::to_string(h_level) + ">\n";
            continue;
        }

        std::string processed;
        for (size_t i = 0; i < line.size(); i++) {
            if (line[i] == '<') processed += "&lt;";
            else if (line[i] == '>') processed += "&gt;";
            else if (line[i] == '&') processed += "&amp;";
            else processed += line[i];
        }

        std::string bold_processed;
        for (size_t i = 0; i < processed.size(); i++) {
            if (i + 1 < processed.size() && processed[i] == '*' && processed[i+1] == '*') {
                i += 2;
                bold_processed += "<strong>";
                while (i + 1 < processed.size() && !(processed[i] == '*' && processed[i+1] == '*')) {
                    bold_processed += processed[i];
                    i++;
                }
                if (i + 1 < processed.size()) { i += 1; } // skip closing ** (+ for-loop i++)
                bold_processed += "</strong>";
            } else {
                bold_processed += processed[i];
            }
        }

        html += "<p>" + bold_processed + "</p>\n";
    }

    if (in_code_block) html += "</code></pre>\n";
    if (in_list) html += "</ul>\n";

    return html;
}
