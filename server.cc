#include "deps/cpp-httplib/httplib.h"
#include "config.h"
#include "db.h"
#include "render.h"
#include "auth.h"
#include "judge.h"
#include <iostream>

int main() {
    std::cout << "Vibe OJ Server starting on port 8080..." << std::endl;
    httplib::Server svr;
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("<h1>Vibe OJ</h1>", "text/html");
    });
    svr.listen("0.0.0.0", 8080);
    return 0;
}
