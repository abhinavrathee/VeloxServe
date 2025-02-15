#pragma once

#include <string>
#include <vector>
#include <map>

// Holds configuration for a single location block
struct LocationConfig {
    std::string path;           // "/api", "/static", "/"
    std::string root;           // Document root override
    std::string index_file = "index.html";
    std::string proxy_pass;     // "http://127.0.0.1:3000" (empty = static)
    std::vector<std::string> methods;  // {"GET", "POST"}
    bool autoindex = false;

    bool is_proxy() const { return !proxy_pass.empty(); }
    bool is_method_allowed(const std::string& method) const;
};

// Holds configuration for a single server block
struct ServerConfig {
    int port = 8080;
    std::string host = "0.0.0.0";
    std::string server_name = "localhost";
    std::string root = "./www";
    size_t client_max_body_size = 1048576;  // 1MB default
    int rate_limit = 0;                      // 0 = disabled
    std::map<int, std::string> error_pages;  // {404: "/errors/404.html"}
    std::vector<LocationConfig> locations;
};

// Holds upstream block (for load balancing)
struct UpstreamConfig {
    std::string name;
    std::vector<std::pair<std::string, int>> servers;  // {host, port} pairs
    int health_check_interval = 5;
};
