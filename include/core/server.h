#pragma once

#include "core/epoll_wrapper.h"
#include "core/thread_pool.h"
#include "core/connection.h"
#include "http/http_parser.h"
#include "http/http_response.h"
#include "http/router.h"
#include "config/server_config.h"
#include "modules/static_handler.h"
#include "modules/load_balancer.h"
#include "middleware/rate_limiter.h"
#include "middleware/cache.h"
#include "middleware/logger.h"

#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <map>
#include <mutex>

// Main server class — owns the listening socket, epoll loop, thread pool,
// and all active client connections. This is the heart of VeloxServe.
class Server {
public:
    explicit Server(int port, const std::string& host = "0.0.0.0",
                    size_t thread_count = 0);
    explicit Server(const ServerConfig& config,
                    const std::map<std::string, UpstreamConfig>& upstreams = {},
                    size_t thread_count = 0);
    ~Server();

    // No copy
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // Create socket, bind, listen, run event loop (blocks until stop)
    bool start();

    // Signal the event loop to stop
    void stop();

    bool is_running() const { return running_.load(); }

private:
    // ---- Event loop ----
    void event_loop();

    // ---- Connection lifecycle ----
    void handle_accept();                               // New client connecting
    void handle_read(int client_fd);                    // Client sent data
    void handle_write(int client_fd);                   // Ready to send response
    void process_request(std::shared_ptr<Connection> conn);  // Parse + route + respond
    std::string get_client_ip(int client_fd);  // Extract IP for logging
    void setup_routes();                       // Register URL routes
    void close_connection(int client_fd);               // Cleanup + close
    void cleanup_inactive_connections();                 // Timeout old connections

    // ---- Members ----
    int server_fd_ = -1;
    int port_;
    std::string host_;
    std::atomic<bool> running_{false};

    std::unique_ptr<EpollWrapper> epoll_;
    std::unique_ptr<ThreadPool> thread_pool_;
    StaticHandler static_handler_;
    Router router_;
    ServerConfig config_;           // Parsed config (from file or defaults)
    std::map<std::string, std::shared_ptr<LoadBalancer>> load_balancers_;
    RateLimiter rate_limiter_;
    LRUCache lru_cache_;
    Logger logger_;

    std::unordered_map<int, std::shared_ptr<Connection>> connections_;
    std::mutex connections_mutex_;

    // ---- Tuning constants ----
    static constexpr int BACKLOG = 1024;             // listen() backlog
    static constexpr int BUFFER_SIZE = 4096;         // recv() buffer size
    static constexpr int TIMEOUT_SECONDS = 30;       // idle connection timeout
    static constexpr size_t MAX_CONNECTIONS = 10000;  // max concurrent clients
    static constexpr size_t MAX_REQUEST_SIZE = 64 * 1024;  // 64KB max request
};
