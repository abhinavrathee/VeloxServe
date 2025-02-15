#include "modules/proxy_handler.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

// ---------------------------------------------------------------------------
// BackendAddr::parse()
// ---------------------------------------------------------------------------
BackendAddr BackendAddr::parse(const std::string& url) {
    BackendAddr addr{"", 80};
    std::string url_no_scheme = url;

    // Strip http:// prefix
    if (url.find("http://") == 0) {
        url_no_scheme = url.substr(7);
    }

    size_t colon = url_no_scheme.find(':');
    if (colon != std::string::npos) {
        addr.host = url_no_scheme.substr(0, colon);
        try {
            addr.port = std::stoi(url_no_scheme.substr(colon + 1));
        } catch (...) {
            addr.port = 80;
        }
    } else {
        addr.host = url_no_scheme;
    }

    // Default localhost to 127.0.0.1 for consistent networking
    if (addr.host == "localhost") {
        addr.host = "127.0.0.1";
    }

    return addr;
}

// ---------------------------------------------------------------------------
// ProxyHandler::handle() — main workflow
// ---------------------------------------------------------------------------
HttpResponse ProxyHandler::handle(const HttpRequest& client_request,
                                  const std::string& proxy_pass_url,
                                  const std::string& client_ip) {
    BackendAddr backend = BackendAddr::parse(proxy_pass_url);

    int backend_fd = connect_to_backend(backend.host, backend.port);
    if (backend_fd == -1) {
        return HttpResponse::internal_error("502 Bad Gateway: Failed to connect to backend");
    }

    std::string host_header = backend.host + ":" + std::to_string(backend.port);
    std::string proxy_req_str = build_backend_request(client_request, host_header, client_ip);

    std::string raw_response = send_and_receive(backend_fd, proxy_req_str);
    close(backend_fd);

    if (raw_response.empty()) {
        return HttpResponse::internal_error("502 Bad Gateway: Backend closed connection");
    }

    HttpResponse response = parse_backend_response(raw_response);
    response.set_header("X-Proxy", "VeloxServe");

    return response;
}

// ---------------------------------------------------------------------------
// connect_to_backend() — sync blocking connect with timeout
// ---------------------------------------------------------------------------
int ProxyHandler::connect_to_backend(const std::string& host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) return -1;

    // Set send/recv timeouts to prevent proxy from hanging indefinitely
    struct timeval tv;
    tv.tv_sec = 5;  // 5 seconds timeout
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        close(fd);
        return -1;
    }

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        close(fd);
        return -1;
    }

    return fd;
}

// ---------------------------------------------------------------------------
// build_backend_request() — serialize request with X-Forwarded headers
// ---------------------------------------------------------------------------
std::string ProxyHandler::build_backend_request(const HttpRequest& req,
                                                const std::string& backend_host,
                                                const std::string& client_ip) {
    std::string out = req.method + " " + req.path;
    if (!req.query_string.empty()) {
        out += "?" + req.query_string;
    }
    out += " HTTP/1.1\r\n";

    // Write Host header to point to backend, not our server
    out += "Host: " + backend_host + "\r\n";

    // Add Proxy identity headers
    out += "X-Forwarded-For: " + client_ip + "\r\n";
    out += "X-Real-IP: " + client_ip + "\r\n";
    out += "X-Forwarded-Proto: http\r\n";
    out += "Connection: close\r\n"; // keep simple for backend

    // Relay original headers (excluding Host and Connection)
    for (const auto& [key, val] : req.headers) {
        if (key != "host" && key != "connection") {
            out += key + ": " + val + "\r\n";
        }
    }
    out += "\r\n";
    out += req.body;

    return out;
}

// ---------------------------------------------------------------------------
// send_and_receive() — blocking socket I/O with backend
// ---------------------------------------------------------------------------
std::string ProxyHandler::send_and_receive(int backend_fd, const std::string& request) {
    size_t total_sent = 0;
    while (total_sent < request.size()) {
        ssize_t sent = send(backend_fd, request.data() + total_sent,
                            request.size() - total_sent, MSG_NOSIGNAL);
        if (sent <= 0) return "";
        total_sent += sent;
    }

    std::string response;
    char buffer[4096];
    while (true) {
        ssize_t received = recv(backend_fd, buffer, sizeof(buffer), 0);
        if (received > 0) {
            response.append(buffer, received);
        } else if (received == 0) {
            break; // backend closed Connection
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout
                break;
            }
            break; // error
        }
    }

    return response;
}

// ---------------------------------------------------------------------------
// parse_backend_response() — parse the raw HTTP text returned by backend
// ---------------------------------------------------------------------------
HttpResponse ProxyHandler::parse_backend_response(const std::string& raw_response) {
    HttpResponse resp;

    size_t header_end = raw_response.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        resp.set_status(502, "Bad Gateway");
        resp.set_body("Invalid response from upstream");
        return resp;
    }

    std::string header_section = raw_response.substr(0, header_end);
    std::string body_section = raw_response.substr(header_end + 4);

    size_t first_line_end = header_section.find("\r\n");
    if (first_line_end == std::string::npos) {
        first_line_end = header_section.size();
    }
    std::string status_line = header_section.substr(0, first_line_end);

    // Parse "HTTP/1.1 200 OK"
    size_t space1 = status_line.find(' ');
    size_t space2 = status_line.find(' ', space1 + 1);

    if (space1 != std::string::npos && space2 != std::string::npos) {
        try {
            int code = std::stoi(status_line.substr(space1 + 1, space2 - space1 - 1));
            std::string reason = status_line.substr(space2 + 1);
            resp.set_status(code, reason);
        } catch (...) {
            resp.set_status(502, "Bad Gateway");
        }
    } else {
        resp.set_status(502, "Bad Gateway");
    }

    // Process remaining headers
    if (first_line_end < header_section.size()) {
        std::string headers = header_section.substr(first_line_end + 2);
        size_t pos = 0;
        while (pos < headers.size()) {
            size_t line_end = headers.find("\r\n", pos);
            if (line_end == std::string::npos) line_end = headers.size();

            std::string line = headers.substr(pos, line_end - pos);
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string val = line.substr(colon + 1);

                // Trim leading spaces from value
                size_t val_start = val.find_first_not_of(" \t");
                if (val_start != std::string::npos) {
                    val = val.substr(val_start);
                }

                resp.set_header(key, val);
            }
            pos = line_end + 2;
        }
    }

    resp.set_body(body_section);
    return resp;
}
