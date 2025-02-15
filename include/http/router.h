#pragma once

#include "http/http_parser.h"
#include "http/http_response.h"

#include <string>
#include <vector>
#include <functional>

// URL prefix-based router. Matches request path against registered routes
// (longest prefix first) and invokes the corresponding handler.
class Router {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    // Register a route: all requests starting with 'prefix' go to 'handler'.
    // allowed_methods: which HTTP methods this route accepts (empty = all).
    void add_route(const std::string& prefix,
                   Handler handler,
                   const std::vector<std::string>& allowed_methods = {});

    // Set fallback handler for unmatched routes (default returns 404)
    void set_default_handler(Handler handler);

    // Match request against routes and invoke handler
    HttpResponse route(const HttpRequest& request);

private:
    struct Route {
        std::string prefix;
        Handler handler;
        std::vector<std::string> allowed_methods;
    };

    std::vector<Route> routes_;     // Sorted longest-prefix-first
    Handler default_handler_;       // Fallback for no match
};
