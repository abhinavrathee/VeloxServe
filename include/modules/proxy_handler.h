#pragma once

#include "http/http_parser.h"
#include "http/http_response.h"
#include <string>

// Struct to represent a parsed backend address
struct BackendAddr {
    std::string host;
    int port;

    // Parses URLs like "http://127.0.0.1:3000" or "http://localhost"
    static BackendAddr parse(const std::string& url);
};

// Proxies HTTP requests to a backend server.
class ProxyHandler {
public:
    // Forwards the client_request to proxy_pass_url, returns backend response
    HttpResponse handle(const HttpRequest& client_request,
                        const std::string& proxy_pass_url,
                        const std::string& client_ip);

private:
    int connect_to_backend(const std::string& host, int port);
    std::string build_backend_request(const HttpRequest& req,
                                      const std::string& backend_host,
                                      const std::string& client_ip);
    std::string send_and_receive(int backend_fd, const std::string& request);
    HttpResponse parse_backend_response(const std::string& raw_response);
};
