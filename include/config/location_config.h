#pragma once

#include <string>
#include <vector>

/**
 * @struct LocationConfig
 * @brief Configuration model for a single `location` block inside veloxserve.conf.
 *
 * Each LocationConfig maps a URI prefix to either a static file root directory
 * or a reverse proxy upstream target. The config parser populates these structs
 * from the declarative configuration file at startup.
 *
 * Example config block:
 *   location /api {
 *       proxy_pass http://127.0.0.1:3000;
 *       methods GET POST PUT DELETE;
 *   }
 */
struct LocationConfig {
    /// The URI path prefix this location matches (e.g., "/", "/api", "/static")
    std::string path;

    /// Document root override for static file serving (empty = inherit from server block)
    std::string root;

    /// Default index file to serve when a directory is requested
    std::string index_file = "index.html";

    /// Upstream proxy target URL (empty = serve static files instead of proxying)
    /// Format: "http://host:port" or "http://upstream_name"
    std::string proxy_pass;

    /// Allowed HTTP methods for this location (e.g., {"GET", "POST"})
    std::vector<std::string> methods;

    /// Whether to auto-generate directory listings (not yet implemented)
    bool autoindex = false;

    /// Returns true if this location is configured as a reverse proxy
    bool is_proxy() const { return !proxy_pass.empty(); }

    /// Returns true if the given HTTP method is in the allowed methods list
    bool is_method_allowed(const std::string& method) const;
};
