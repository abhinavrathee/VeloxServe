#pragma once

#include <string>
#include <unordered_map>
#include <cstddef>

// Holds a fully parsed HTTP request — method, path, headers, body.
struct HttpRequest {
    std::string method;        // GET, POST, PUT, DELETE, HEAD
    std::string path;          // /index.html (decoded, no query string)
    std::string query_string;  // key=val&foo=bar (raw, after '?')
    std::string version;       // HTTP/1.1
    std::string client_ip;     // Extracted from socket for logging/proxy
    std::unordered_map<std::string, std::string> headers;  // keys lowercased
    std::string body;
    bool is_complete = false;
    bool is_valid = false;

    // Case-insensitive header lookup (keys are already lowered)
    std::string get_header(const std::string& key,
                           const std::string& default_val = "") const;

    // Does client want keep-alive?
    bool wants_keep_alive() const;

    // Returns Content-Length header value, 0 if missing
    size_t content_length() const;
};

// Incremental HTTP/1.1 request parser.
// Can handle partial data across multiple recv() calls.
class HttpParser {
public:
    // Feed raw bytes. Returns true when a complete request is ready.
    bool parse(const std::string& data, HttpRequest& request);

    // Reset state for next request (keep-alive reuse)
    void reset();

private:
    bool parse_request_line(const std::string& line, HttpRequest& req);
    bool parse_headers(const std::string& header_section, HttpRequest& req);

    static std::string url_decode(const std::string& str);
    static std::string trim(const std::string& str);
    static std::string to_lower(const std::string& str);
    static bool is_valid_method(const std::string& method);

    std::string buffer_;
    bool headers_parsed_ = false;
    size_t content_length_ = 0;
};
