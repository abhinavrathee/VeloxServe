#pragma once

#include "http/http_parser.h"
#include "http/http_response.h"
#include "middleware/cache.h"
#include <string>

// Serves static files from a document root directory.
// Handles path traversal prevention, MIME types, directory index, and 404s.
class StaticHandler {
public:
    // Handle a request — map path to file, read it, return response
    HttpResponse handle(const HttpRequest& request,
                        const std::string& root_dir,
                        const std::string& index_file = "index.html",
                        LRUCache* cache = nullptr);

private:
    // Security: prevent /../ path traversal attacks
    // Returns sanitized path or empty string if blocked
    std::string sanitize_path(const std::string& request_path,
                              const std::string& root_dir);

    // Read entire file into string (binary-safe)
    bool read_file(const std::string& filepath, std::string& out_content);

    // Check if path is a directory
    bool is_directory(const std::string& path);

    // Check if file exists
    bool file_exists(const std::string& path);
};
