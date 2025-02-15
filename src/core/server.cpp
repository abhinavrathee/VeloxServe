#include "core/server.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <chrono>
#include <vector>

#include "modules/proxy_handler.h"
#include <filesystem>
#include <sstream>

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
Server::Server(int port, const std::string& host, size_t thread_count)
    : port_(port), host_(host)
{
    epoll_   = std::make_unique<EpollWrapper>();
    thread_pool_ = std::make_unique<ThreadPool>(thread_count);
}

Server::Server(const ServerConfig& config,
               const std::map<std::string, UpstreamConfig>& upstreams,
               size_t thread_count)
    : port_(config.port), host_(config.host), config_(config),
      rate_limiter_(config.rate_limit > 0 ? config.rate_limit : 1000000.0,
                    config.rate_limit > 0 ? config.rate_limit * 2.0 : 200.0,
                    config.rate_limit > 0)
{
    epoll_   = std::make_unique<EpollWrapper>();
    thread_pool_ = std::make_unique<ThreadPool>(thread_count);

    // Initialize load balancers
    for (const auto& [name, upstream] : upstreams) {
        auto lb = std::make_shared<LoadBalancer>();
        for (const auto& server : upstream.servers) {
            lb->add_backend(server.first, server.second);
        }
        lb->start_health_checks(upstream.health_check_interval);
        load_balancers_[name] = lb;
        std::cout << "[VeloxServe] Upstream '" << name << "' initialized with "
                  << upstream.servers.size() << " backends" << std::endl;
    }

    if (config_.rate_limit > 0) {
        std::cout << "[VeloxServe] Rate limiter enabled: " << config_.rate_limit << " req/s" << std::endl;
    }

    // Ensure logs directory exists
    std::filesystem::create_directories("logs");
}

Server::~Server() {
    stop();
}

// ---------------------------------------------------------------------------
// start() — Create socket, bind, listen, enter event loop
// ---------------------------------------------------------------------------
bool Server::start() {
    // 1. Create TCP socket (non-blocking from the start)
    server_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_fd_ == -1) {
        std::cerr << "[VeloxServe] socket() failed: " << strerror(errno) << std::endl;
        return false;
    }

    // 2. Socket options for production use
    int opt = 1;

    //  SO_REUSEADDR — allows restart without "Address already in use"
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "[VeloxServe] SO_REUSEADDR failed: " << strerror(errno) << std::endl;
        close(server_fd_);
        return false;
    }

    //  SO_REUSEPORT — allows multiple processes to bind to same port (optional)
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    //  TCP_NODELAY — disable Nagle's algorithm for lower latency
    setsockopt(server_fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    // 3. Bind to address:port
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port_);

    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0) {
        // If host string is invalid, bind to all interfaces
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        std::cerr << "[VeloxServe] bind() failed on port " << port_
                  << ": " << strerror(errno) << std::endl;
        close(server_fd_);
        return false;
    }

    // 4. Listen — BACKLOG = max pending connections in kernel queue
    if (listen(server_fd_, BACKLOG) == -1) {
        std::cerr << "[VeloxServe] listen() failed: " << strerror(errno) << std::endl;
        close(server_fd_);
        return false;
    }

    // 5. Register server socket with epoll for incoming connections
    if (!epoll_->add(server_fd_, EPOLLIN)) {
        std::cerr << "[VeloxServe] Failed to add server fd to epoll" << std::endl;
        close(server_fd_);
        return false;
    }

    // 6. We're live
    running_.store(true);
    std::cout << "\n"
              << "╔══════════════════════════════════════════╗\n"
              << "║         VeloxServe is running!           ║\n"
              << "║  Listening on " << host_ << ":" << port_
              << std::string(27 - host_.size() - std::to_string(port_).size(), ' ') << "║\n"
              << "║  Press Ctrl+C to stop                    ║\n"
              << "╚══════════════════════════════════════════╝\n"
              << std::endl;

    // 7. Setup URL routes
    setup_routes();

    // 8. Enter the main event loop (blocks here until stop() is called)
    event_loop();

    return true;
}

// ---------------------------------------------------------------------------
// stop() — Graceful shutdown
// ---------------------------------------------------------------------------
void Server::stop() {
    if (!running_.load()) return;
    running_.store(false);

    std::cout << "\n[VeloxServe] Shutting down..." << std::endl;

    // 1. Stop thread pool first (finishes current tasks, rejects new ones)
    if (thread_pool_) {
        thread_pool_->shutdown();
    }

    // 2. Close all client connections
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto& [fd, conn] : connections_) {
            epoll_->remove(fd);
            close(fd);
        }
        connections_.clear();
    }

    // 3. Stop load balancer health checks
    for (auto& [name, lb] : load_balancers_) {
        lb->stop_health_checks();
    }

    // 4. Close listening socket
    if (server_fd_ != -1) {
        epoll_->remove(server_fd_);
        close(server_fd_);
        server_fd_ = -1;
    }

    std::cout << "[VeloxServe] Shutdown complete." << std::endl;
}

// ---------------------------------------------------------------------------
// event_loop() — Heart of the server. Waits for epoll events, dispatches.
// ---------------------------------------------------------------------------
void Server::event_loop() {
    while (running_.load()) {
        // Wait up to 1 second — allows periodic timeout checks + shutdown detection
        int num_events = epoll_->wait(1000);

        if (num_events < 0) {
            continue;  // error already logged inside wait()
        }

        for (int i = 0; i < num_events; ++i) {
            const auto& event = epoll_->get_event(i);
            int fd = event.data.fd;

            if (fd == server_fd_) {
                // Server socket: new client trying to connect
                if (event.events & EPOLLIN) {
                    handle_accept();
                }
            } else {
                // Client socket
                if (event.events & (EPOLLERR | EPOLLHUP)) {
                    // Error or hangup — close immediately
                    close_connection(fd);
                } else if (event.events & EPOLLIN) {
                    // Client sent data — read it
                    handle_read(fd);
                } else if (event.events & EPOLLOUT) {
                    // Socket is writable — send pending response
                    handle_write(fd);
                }
            }
        }

        // Periodically clean up idle connections
        cleanup_inactive_connections();
    }
}

// ---------------------------------------------------------------------------
// handle_accept() — Accept all pending connections (loop for edge-triggered)
// ---------------------------------------------------------------------------
void Server::handle_accept() {
    while (true) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        // accept4 with SOCK_NONBLOCK — new socket is non-blocking immediately
        int client_fd = accept4(server_fd_,
                                reinterpret_cast<struct sockaddr*>(&client_addr),
                                &client_len, SOCK_NONBLOCK);

        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // No more pending connections — done
            }
            std::cerr << "[VeloxServe] accept4() failed: " << strerror(errno) << std::endl;
            break;
        }

        // Check connection limit
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            if (connections_.size() >= MAX_CONNECTIONS) {
                std::cerr << "[VeloxServe] Connection limit reached ("
                          << MAX_CONNECTIONS << "), rejecting" << std::endl;
                close(client_fd);
                continue;
            }
        }

        // TCP_NODELAY on client socket for low latency
        int opt = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

        // Register with epoll: watch for readable data + errors
        if (!epoll_->add(client_fd, EPOLLIN | EPOLLHUP | EPOLLERR)) {
            close(client_fd);
            continue;
        }

        // Create Connection object and store it
        auto conn = std::make_shared<Connection>(client_fd);
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_[client_fd] = conn;
        }
    }
}

// ---------------------------------------------------------------------------
// handle_read() — Read data from client, dispatch to thread pool for processing
// ---------------------------------------------------------------------------
void Server::handle_read(int client_fd) {
    std::shared_ptr<Connection> conn;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(client_fd);
        if (it == connections_.end()) return;  // already closed
        conn = it->second;
    }

    // Read all available data (loop for edge-triggered epoll)
    char buffer[BUFFER_SIZE];
    while (true) {
        ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);

        if (bytes > 0) {
            // Check request size limit
            if (conn->read_buffer.size() + bytes > MAX_REQUEST_SIZE) {
                std::cerr << "[VeloxServe] Request too large, closing fd="
                          << client_fd << std::endl;
                close_connection(client_fd);
                return;
            }
            conn->read_buffer.append(buffer, bytes);
            conn->last_activity = std::chrono::steady_clock::now();
        } else if (bytes == 0) {
            // Client closed the connection gracefully
            close_connection(client_fd);
            return;
        } else {
            // bytes == -1
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // No more data available right now — done reading
            }
            // Real error
            close_connection(client_fd);
            return;
        }
    }

    // Check if we have a complete HTTP request (headers end with \r\n\r\n)
    if (conn->read_buffer.find("\r\n\r\n") != std::string::npos) {
        // Dispatch processing to thread pool — don't block the event loop
        thread_pool_->enqueue([this, conn]() {
            process_request(conn);
        });
    }
}

// ---------------------------------------------------------------------------
// process_request() — Parse HTTP request, serve static files or proxy
// Runs in a thread pool worker — must NOT block the event loop.
// ---------------------------------------------------------------------------
void Server::process_request(std::shared_ptr<Connection> conn) {
    auto start_time = std::chrono::steady_clock::now();

    HttpParser parser;
    HttpRequest request;
    HttpResponse response;

    // Extract client IP before parsing
    request.client_ip = get_client_ip(conn->fd);

    // Rate Limiter Check
    if (!rate_limiter_.is_allowed(request.client_ip)) {
        response = HttpResponse::ok("429 Too Many Requests");
        response.set_status(429, "Too Many Requests");
        response.set_header("Retry-After", "1");
    } 
    // Parse the raw buffer into a structured HttpRequest
    else if (!parser.parse(conn->read_buffer, request) || !request.is_valid) {
        response = HttpResponse::bad_request("Invalid HTTP request");
    } else {
        // Update keep-alive from parsed request
        conn->keep_alive = request.wants_keep_alive();

        // Route through the router — it handles method checks + dispatch
        response = router_.route(request);

        // For HEAD requests: keep headers, drop the body
        if (request.method == "HEAD") {
            response.set_body("");
        }

        // Apply custom error page if defined in config
        if (response.status_code() >= 400 && config_.error_pages.count(response.status_code())) {
            std::string err_path = config_.root + config_.error_pages[response.status_code()];
            // Try to serve it using static handler (which handles cache + MIME)
            HttpRequest err_req;
            err_req.path = config_.error_pages[response.status_code()];
            HttpResponse err_resp = static_handler_.handle(err_req, config_.root, "index.html", &lru_cache_);
            if (err_resp.status_code() == 200) {
                // Keep the original error status code, but use the file body
                response.set_body(err_resp.body());
                response.set_header("Content-Type", err_resp.get_header("content-type", "text/html"));
            }
        }

        // Set keep-alive header
        if (conn->keep_alive) {
            response.set_header("Connection", "keep-alive");
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    int duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    // Log the request
    logger_.log_access(request, response, request.client_ip, duration_ms);

    // Serialize response and queue for sending
    conn->write_buffer = response.build();
    conn->bytes_sent = 0;
    conn->is_writing = true;
    conn->read_buffer.clear();

    // Tell epoll: switch this fd from read-mode to write-mode
    epoll_->modify(conn->fd, EPOLLOUT | EPOLLHUP | EPOLLERR);
}

// ---------------------------------------------------------------------------
// get_client_ip() — Extract IP address string from connected socket
// ---------------------------------------------------------------------------
std::string Server::get_client_ip(int client_fd) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(client_fd, reinterpret_cast<struct sockaddr*>(&addr), &addr_len) == 0) {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
        return std::string(ip_str);
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// setup_routes() — Register all URL routes + Internal endpoints
// ---------------------------------------------------------------------------
void Server::setup_routes() {
    // 1. Internal status routes
    router_.add_route("/health", [](const HttpRequest& req) {
        (void)req;
        return HttpResponse::ok("OK\n");
    }, {"GET"});

    router_.add_route("/metrics", [this](const HttpRequest& req) {
        (void)req;
        std::stringstream ss;
        ss << "# VeloxServe Metrics\n";
        
        size_t rl_total, rl_blocked, rl_ips;
        rate_limiter_.get_stats(rl_total, rl_blocked, rl_ips);
        ss << "rate_limit_total_requests " << rl_total << "\n";
        ss << "rate_limit_blocked_requests " << rl_blocked << "\n";
        ss << "rate_limit_active_ips " << rl_ips << "\n";

        size_t c_hits, c_misses, c_entries, c_mem;
        lru_cache_.get_stats(c_hits, c_misses, c_entries, c_mem);
        ss << "cache_hits " << c_hits << "\n";
        ss << "cache_misses " << c_misses << "\n";
        ss << "cache_entries " << c_entries << "\n";
        ss << "cache_memory_bytes " << c_mem << "\n";

        for (const auto& [name, lb] : load_balancers_) {
            ss << "upstream_" << name << "_healthy " << lb->healthy_count() << "\n";
            ss << "upstream_" << name << "_total " << lb->backend_count() << "\n";
        }

        return HttpResponse::ok(ss.str());
    }, {"GET"});
    std::cout << "[VeloxServe] Internal routes: /health, /metrics" << std::endl;

    // 2. Build routes from config locations (if config has locations)
    if (!config_.locations.empty()) {
        for (const auto& loc : config_.locations) {
            if (loc.is_proxy()) {
                // Proxy location — handles both single URL and Upstream load balancing
                std::string proxy_target = loc.proxy_pass;

                // Check if target is an upstream name
                std::string upstream_name = proxy_target;
                if (upstream_name.find("http://") == 0) upstream_name = upstream_name.substr(7);

                if (load_balancers_.count(upstream_name)) {
                    // It's a load balancer upstream
                    auto lb = load_balancers_[upstream_name];
                    router_.add_route(loc.path, [lb](const HttpRequest& req) {
                        Backend* backend = lb->get_next();
                        if (!backend) {
                            return HttpResponse::internal_error("503 Service Unavailable: No healthy backends");
                        }

                        ProxyHandler proxy;
                        std::string target = "http://" + backend->host + ":" + std::to_string(backend->port);
                        HttpResponse resp = proxy.handle(req, target, req.client_ip);

                        if (resp.status_code() == 502) {
                            lb->mark_down(backend);
                        }
                        lb->release(backend);
                        return resp;
                    }, loc.methods);
                    std::cout << "  " << loc.path << " \t→ load_balancer: " << upstream_name << std::endl;
                } else {
                    // Direct single proxy pass
                    router_.add_route(loc.path, [proxy_target](const HttpRequest& req) {
                        ProxyHandler proxy;
                        return proxy.handle(req, proxy_target, req.client_ip);
                    }, loc.methods);
                    std::cout << "  " << loc.path << " \t→ proxy: " << proxy_target << std::endl;
                }
            } else {
                // Static file location
                std::string root = loc.root.empty() ? config_.root : loc.root;
                std::string idx  = loc.index_file;
                router_.add_route(loc.path, [this, root, idx](const HttpRequest& req) {
                    return static_handler_.handle(req, root, idx, &lru_cache_);
                }, loc.methods);
                std::cout << "  " << loc.path << " \t→ static: " << root << std::endl;
            }
        }
    } else {
        // No config locations — default route
        router_.add_route("/", [this](const HttpRequest& req) {
            return static_handler_.handle(req, "./www", "index.html", &lru_cache_);
        }, {"GET", "HEAD"});
        std::cout << "  /        \t→ static: ./www (default)" << std::endl;
    }

    std::cout << "[VeloxServe] " << (config_.locations.empty() ? 1 : config_.locations.size())
              << " route(s) registered." << std::endl;
}

// ---------------------------------------------------------------------------
// handle_write() — Send pending response data to client
// ---------------------------------------------------------------------------
void Server::handle_write(int client_fd) {
    std::shared_ptr<Connection> conn;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(client_fd);
        if (it == connections_.end()) return;
        conn = it->second;
    }

    if (conn->write_buffer.empty()) {
        close_connection(client_fd);
        return;
    }

    // Send as much as possible (loop for partial writes)
    while (conn->bytes_sent < conn->write_buffer.size()) {
        size_t remaining = conn->write_buffer.size() - conn->bytes_sent;
        ssize_t sent = send(client_fd,
                            conn->write_buffer.data() + conn->bytes_sent,
                            remaining,
                            MSG_NOSIGNAL);  // MSG_NOSIGNAL prevents SIGPIPE

        if (sent > 0) {
            conn->bytes_sent += sent;
        } else if (sent == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Socket buffer full — wait for next EPOLLOUT
                return;
            }
            // Real error (EPIPE, ECONNRESET, etc.)
            close_connection(client_fd);
            return;
        } else {
            // sent == 0 — shouldn't happen for stream sockets
            close_connection(client_fd);
            return;
        }
    }

    // All data sent successfully
    conn->write_buffer.clear();
    conn->bytes_sent = 0;
    conn->is_writing = false;

    if (conn->keep_alive) {
        // Switch back to reading for next request
        epoll_->modify(client_fd, EPOLLIN | EPOLLHUP | EPOLLERR);
    } else {
        close_connection(client_fd);
    }
}

// ---------------------------------------------------------------------------
// close_connection() — Remove from epoll, close socket, remove from map
// ---------------------------------------------------------------------------
void Server::close_connection(int client_fd) {
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(client_fd);
        if (it == connections_.end()) return;  // already closed
        connections_.erase(it);
    }

    epoll_->remove(client_fd);
    close(client_fd);
}

// ---------------------------------------------------------------------------
// cleanup_inactive_connections() — Close connections idle > TIMEOUT_SECONDS
// ---------------------------------------------------------------------------
void Server::cleanup_inactive_connections() {
    auto now = std::chrono::steady_clock::now();
    std::vector<int> stale_fds;

    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (const auto& [fd, conn] : connections_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - conn->last_activity);
            if (elapsed.count() > TIMEOUT_SECONDS && !conn->is_writing) {
                stale_fds.push_back(fd);
            }
        }
    }

    for (int fd : stale_fds) {
        close_connection(fd);
    }

    // Also tell rate limiter to clear old IP buckets
    rate_limiter_.cleanup_expired();
}
