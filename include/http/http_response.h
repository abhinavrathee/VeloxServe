#pragma once

#include <string>
#include <unordered_map>
#include <vector>

// Builds a well-formed HTTP/1.1 response string.
class HttpResponse {
public:
    HttpResponse() = default;
    HttpResponse(int code, const std::string& reason);

    void set_status(int code, const std::string& reason);
    void set_header(const std::string& key, const std::string& value);
    std::string get_header(const std::string& key, const std::string& default_val = "") const;
    void set_body(const std::string& body);
    void set_body(const std::vector<char>& body);

    int status_code() const { return status_code_; }
    const std::string& body() const { return body_; }

    // Serialize to full HTTP response string ready to send
    std::string build() const;

    // ---- Convenience factory methods ----
    static HttpResponse ok(const std::string& body,
                           const std::string& content_type = "text/plain");
    static HttpResponse not_found(const std::string& body = "");
    static HttpResponse bad_request(const std::string& body = "400 Bad Request");
    static HttpResponse forbidden(const std::string& body = "403 Forbidden");
    static HttpResponse method_not_allowed(const std::string& allowed_methods);
    static HttpResponse internal_error(const std::string& body = "500 Internal Server Error");
    static HttpResponse redirect(const std::string& location);
    static HttpResponse json(const std::string& json_body);

private:
    int status_code_ = 200;
    std::string status_text_ = "OK";
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};
