#include "http/http_parser.h"
#include <algorithm>
#include <sstream>
#include <cctype>

// ---------------------------------------------------------------------------
// HttpRequest helper methods
// ---------------------------------------------------------------------------
std::string HttpRequest::get_header(const std::string& key,
                                    const std::string& default_val) const {
    // Headers are stored lowercase
    std::string lower_key = key;
    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);
    auto it = headers.find(lower_key);
    return (it != headers.end()) ? it->second : default_val;
}

bool HttpRequest::wants_keep_alive() const {
    std::string conn = get_header("connection");
    std::string lower_conn = conn;
    std::transform(lower_conn.begin(), lower_conn.end(), lower_conn.begin(), ::tolower);

    if (version == "HTTP/1.1") {
        // HTTP/1.1 defaults to keep-alive unless "Connection: close"
        return lower_conn != "close";
    }
    // HTTP/1.0 defaults to close unless "Connection: keep-alive"
    return lower_conn == "keep-alive";
}

size_t HttpRequest::content_length() const {
    std::string val = get_header("content-length");
    if (val.empty()) return 0;
    try {
        return std::stoul(val);
    } catch (...) {
        return 0;
    }
}

// ---------------------------------------------------------------------------
// HttpParser — incremental parsing
// ---------------------------------------------------------------------------
bool HttpParser::parse(const std::string& data, HttpRequest& request) {
    buffer_ += data;

    // Stage 1: Parse headers (look for \r\n\r\n boundary)
    if (!headers_parsed_) {
        size_t header_end = buffer_.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            return false;  // Need more data — headers incomplete
        }

        std::string header_section = buffer_.substr(0, header_end);

        // Split into request line (first line) and remaining headers
        size_t first_line_end = header_section.find("\r\n");
        std::string request_line;
        std::string remaining_headers;

        if (first_line_end != std::string::npos) {
            request_line = header_section.substr(0, first_line_end);
            remaining_headers = header_section.substr(first_line_end + 2);
        } else {
            request_line = header_section;
        }

        if (!parse_request_line(request_line, request)) {
            request.is_valid = false;
            return true;  // Return true = done parsing (invalid request)
        }

        if (!remaining_headers.empty()) {
            if (!parse_headers(remaining_headers, request)) {
                request.is_valid = false;
                return true;
            }
        }

        headers_parsed_ = true;

        // Remove header portion from buffer, keep only body bytes
        buffer_ = buffer_.substr(header_end + 4);  // +4 for \r\n\r\n

        // Extract content-length for body handling
        auto it = request.headers.find("content-length");
        if (it != request.headers.end()) {
            try {
                content_length_ = std::stoul(it->second);
            } catch (...) {
                content_length_ = 0;
            }
        } else {
            content_length_ = 0;
        }
    }

    // Stage 2: Parse body (if Content-Length > 0)
    if (content_length_ > 0) {
        if (buffer_.size() >= content_length_) {
            request.body = buffer_.substr(0, content_length_);
            buffer_ = buffer_.substr(content_length_);
            request.is_complete = true;
            request.is_valid = true;
            return true;
        }
        return false;  // Need more body data
    }

    // No body expected — request is complete
    request.is_complete = true;
    request.is_valid = true;
    return true;
}

void HttpParser::reset() {
    buffer_.clear();
    headers_parsed_ = false;
    content_length_ = 0;
}

// ---------------------------------------------------------------------------
// Parse "GET /path?query HTTP/1.1"
// ---------------------------------------------------------------------------
bool HttpParser::parse_request_line(const std::string& line, HttpRequest& req) {
    std::istringstream stream(line);
    std::string uri;

    if (!(stream >> req.method >> uri >> req.version)) {
        return false;
    }

    if (!is_valid_method(req.method)) return false;
    if (req.version != "HTTP/1.1" && req.version != "HTTP/1.0") return false;

    // Split URI into path and query string at '?'
    size_t query_pos = uri.find('?');
    if (query_pos != std::string::npos) {
        req.path = url_decode(uri.substr(0, query_pos));
        req.query_string = uri.substr(query_pos + 1);
    } else {
        req.path = url_decode(uri);
    }

    // Ensure path starts with /
    if (req.path.empty() || req.path[0] != '/') {
        req.path = "/" + req.path;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Parse header lines: "Key: Value\r\nKey2: Value2\r\n..."
// ---------------------------------------------------------------------------
bool HttpParser::parse_headers(const std::string& header_section, HttpRequest& req) {
    std::istringstream stream(header_section);
    std::string line;

    while (std::getline(stream, line)) {
        // Remove trailing \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) continue;

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;  // skip malformed headers

        std::string key = to_lower(trim(line.substr(0, colon)));
        std::string value = trim(line.substr(colon + 1));

        req.headers[key] = value;
    }
    return true;
}

// ---------------------------------------------------------------------------
// URL decode: %20 → space, %2F → /, + → space
// ---------------------------------------------------------------------------
std::string HttpParser::url_decode(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            // Convert two hex chars to a byte
            char high = str[i + 1];
            char low  = str[i + 2];
            if (std::isxdigit(high) && std::isxdigit(low)) {
                int value = 0;
                std::istringstream iss(str.substr(i + 1, 2));
                iss >> std::hex >> value;
                result += static_cast<char>(value);
                i += 2;  // skip the two hex chars
                continue;
            }
        }
        if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string HttpParser::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string HttpParser::to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

bool HttpParser::is_valid_method(const std::string& method) {
    return method == "GET"  || method == "POST"   || method == "PUT" ||
           method == "DELETE" || method == "HEAD" || method == "OPTIONS";
}
