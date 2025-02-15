#include "http/http_response.h"
#include <sstream>

HttpResponse::HttpResponse(int code, const std::string& reason)
    : status_code_(code), status_text_(reason) {}

void HttpResponse::set_status(int code, const std::string& reason) {
    status_code_ = code;
    status_text_ = reason;
}

void HttpResponse::set_header(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

std::string HttpResponse::get_header(const std::string& key, const std::string& default_val) const {
    auto it = headers_.find(key);
    if (it != headers_.end()) {
        return it->second;
    }
    return default_val;
}

void HttpResponse::set_body(const std::string& body) {
    body_ = body;
}

void HttpResponse::set_body(const std::vector<char>& body) {
    body_.assign(body.begin(), body.end());
}

// Serialize to: "HTTP/1.1 200 OK\r\nHeaders...\r\n\r\nBody"
std::string HttpResponse::build() const {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << status_code_ << " " << status_text_ << "\r\n";

    // Write all headers
    for (const auto& [key, value] : headers_) {
        ss << key << ": " << value << "\r\n";
    }

    // Auto-add Content-Length if not set
    if (headers_.find("Content-Length") == headers_.end()) {
        ss << "Content-Length: " << body_.size() << "\r\n";
    }

    // Auto-add Server header
    if (headers_.find("Server") == headers_.end()) {
        ss << "Server: VeloxServe/1.0\r\n";
    }

    // Auto-add Connection: close if not set
    if (headers_.find("Connection") == headers_.end()) {
        ss << "Connection: close\r\n";
    }

    ss << "\r\n";   // End of headers
    ss << body_;    // Body

    return ss.str();
}

// ---------------------------------------------------------------------------
// Factory methods
// ---------------------------------------------------------------------------
HttpResponse HttpResponse::ok(const std::string& body,
                               const std::string& content_type) {
    HttpResponse resp(200, "OK");
    resp.set_header("Content-Type", content_type);
    resp.set_body(body);
    return resp;
}

HttpResponse HttpResponse::not_found(const std::string& body) {
    HttpResponse resp(404, "Not Found");
    resp.set_header("Content-Type", "text/html");
    std::string b = body.empty()
        ? "<html><body><h1>404 Not Found</h1></body></html>"
        : body;
    resp.set_body(b);
    return resp;
}

HttpResponse HttpResponse::bad_request(const std::string& body) {
    HttpResponse resp(400, "Bad Request");
    resp.set_header("Content-Type", "text/plain");
    resp.set_body(body);
    return resp;
}

HttpResponse HttpResponse::forbidden(const std::string& body) {
    HttpResponse resp(403, "Forbidden");
    resp.set_header("Content-Type", "text/plain");
    resp.set_body(body);
    return resp;
}

HttpResponse HttpResponse::method_not_allowed(const std::string& allowed) {
    HttpResponse resp(405, "Method Not Allowed");
    resp.set_header("Content-Type", "text/plain");
    resp.set_header("Allow", allowed);
    resp.set_body("405 Method Not Allowed");
    return resp;
}

HttpResponse HttpResponse::internal_error(const std::string& body) {
    HttpResponse resp(500, "Internal Server Error");
    resp.set_header("Content-Type", "text/html");
    std::string b = body.empty()
        ? "<html><body><h1>500 Internal Server Error</h1></body></html>"
        : body;
    resp.set_body(b);
    return resp;
}

HttpResponse HttpResponse::redirect(const std::string& location) {
    HttpResponse resp(301, "Moved Permanently");
    resp.set_header("Location", location);
    resp.set_body("");
    return resp;
}

HttpResponse HttpResponse::json(const std::string& json_body) {
    HttpResponse resp(200, "OK");
    resp.set_header("Content-Type", "application/json");
    resp.set_body(json_body);
    return resp;
}
