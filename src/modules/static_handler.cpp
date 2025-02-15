#include "modules/static_handler.h"
#include "http/mime_types.h"

#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <vector>
#include <iostream>

// ---------------------------------------------------------------------------
// handle() — Main entry: map request path to file, serve it
// ---------------------------------------------------------------------------
HttpResponse StaticHandler::handle(const HttpRequest& request,
                                    const std::string& root_dir,
                                    const std::string& index_file,
                                    LRUCache* cache) {
    // 1. Security: sanitize the path
    std::string safe_path = sanitize_path(request.path, root_dir);
    if (safe_path.empty()) {
        return HttpResponse::forbidden("403 Forbidden — Path traversal blocked");
    }

    // 2. Build filesystem path
    std::string fs_path = root_dir + safe_path;

    // 3. If it's a directory, try serving index file
    if (is_directory(fs_path)) {
        // If path doesn't end with /, redirect (prevents relative link issues)
        if (request.path.back() != '/') {
            return HttpResponse::redirect(request.path + "/");
        }
        fs_path += "/" + index_file;
    }

    // 4. Try cache first
    std::string cache_key = fs_path;
    if (cache) {
        auto cached = cache->get(cache_key);
        if (cached) {
            HttpResponse resp(200, "OK");
            resp.set_header("Content-Type", cached->content_type);
            resp.set_header("Content-Length", std::to_string(cached->data.size()));
            resp.set_header("X-Cache", "HIT");
            resp.set_body(cached->data);
            return resp;
        }
    }

    // 5. Check if file exists (cache miss)
    if (!file_exists(fs_path)) {
        return HttpResponse::not_found();
    }

    // 6. Read file contents
    std::string content;
    if (!read_file(fs_path, content)) {
        return HttpResponse::internal_error();
    }

    // 7. Detect MIME type, cache, and build response
    std::string mime = get_mime_type(fs_path);

    if (cache) {
        cache->put(cache_key, content, mime);
    }

    HttpResponse resp(200, "OK");
    resp.set_header("Content-Type", mime);
    resp.set_header("Content-Length", std::to_string(content.size()));
    resp.set_header("X-Cache", "MISS");
    resp.set_body(content);

    return resp;
}

// ---------------------------------------------------------------------------
// sanitize_path() — Prevent /../../../etc/passwd attacks
//
// Algorithm:
//   Split path by '/'
//   For each segment:
//     ".." → go up one level (but never above root)
//     "."  → skip (current directory)
//     else → go deeper
//   Reassemble clean path
//   If result goes above root → block
// ---------------------------------------------------------------------------
std::string StaticHandler::sanitize_path(const std::string& request_path,
                                          const std::string& root_dir) {
    // Split path by '/'
    std::vector<std::string> parts;
    std::istringstream stream(request_path);
    std::string segment;
    int depth = 0;

    while (std::getline(stream, segment, '/')) {
        if (segment.empty() || segment == ".") {
            continue;  // skip empty and current-dir
        }
        if (segment == "..") {
            depth--;
            if (depth < 0) {
                // Tried to go above root — BLOCKED
                std::cerr << "[StaticHandler] Path traversal blocked: "
                          << request_path << std::endl;
                return "";
            }
            if (!parts.empty()) {
                parts.pop_back();
            }
        } else {
            depth++;
            parts.push_back(segment);
        }
    }

    // Reassemble clean path
    std::string clean = "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) clean += "/";
        clean += parts[i];
    }

    return clean;
}

// ---------------------------------------------------------------------------
// read_file() — Binary-safe file reading
// ---------------------------------------------------------------------------
bool StaticHandler::read_file(const std::string& filepath,
                               std::string& out_content) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Read entire file via streambuf iterator
    out_content.assign(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );

    return true;
}

// ---------------------------------------------------------------------------
// is_directory() / file_exists() — stat-based checks
// ---------------------------------------------------------------------------
bool StaticHandler::is_directory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

bool StaticHandler::file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}
