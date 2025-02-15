#include "http/router.h"
#include <algorithm>

// ---------------------------------------------------------------------------
// add_route() — Register a prefix + handler, keep sorted longest first
// ---------------------------------------------------------------------------
void Router::add_route(const std::string& prefix,
                       Handler handler,
                       const std::vector<std::string>& allowed_methods) {
    routes_.push_back({prefix, std::move(handler), allowed_methods});

    // Sort by prefix length descending — ensures /api/v1 matches before /api
    std::sort(routes_.begin(), routes_.end(),
              [](const Route& a, const Route& b) {
                  return a.prefix.size() > b.prefix.size();
              });
}

void Router::set_default_handler(Handler handler) {
    default_handler_ = std::move(handler);
}

// ---------------------------------------------------------------------------
// route() — Find matching route, check method, invoke handler
// ---------------------------------------------------------------------------
HttpResponse Router::route(const HttpRequest& request) {
    for (const auto& route : routes_) {
        // Match: exact path, or path starts with prefix/
        // Special case: "/" matches everything as fallback (it's sorted last)
        bool match = false;

        if (route.prefix == "/") {
            match = true;  // root catches all
        } else if (request.path == route.prefix) {
            match = true;  // exact match
        } else if (request.path.size() > route.prefix.size() &&
                   request.path.substr(0, route.prefix.size()) == route.prefix &&
                   request.path[route.prefix.size()] == '/') {
            match = true;  // prefix match with / boundary
        }

        if (!match) continue;

        // Check if HTTP method is allowed for this route
        if (!route.allowed_methods.empty()) {
            bool method_ok = false;
            for (const auto& m : route.allowed_methods) {
                if (m == request.method) {
                    method_ok = true;
                    break;
                }
            }
            if (!method_ok) {
                // Build "Allow" header value
                std::string allowed;
                for (size_t i = 0; i < route.allowed_methods.size(); ++i) {
                    if (i > 0) allowed += ", ";
                    allowed += route.allowed_methods[i];
                }
                return HttpResponse::method_not_allowed(allowed);
            }
        }

        // Method is OK — invoke handler
        return route.handler(request);
    }

    // No route matched — use default or 404
    if (default_handler_) {
        return default_handler_(request);
    }
    return HttpResponse::not_found();
}
