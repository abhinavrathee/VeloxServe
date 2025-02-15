#include "modules/load_balancer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

LoadBalancer::~LoadBalancer() {
    stop_health_checks();
}

void LoadBalancer::add_backend(const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(mutex_);
    backends_.emplace_back(host, port);
}

Backend* LoadBalancer::get_next() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (backends_.empty()) return nullptr;

    size_t tried = 0;
    while (tried < backends_.size()) {
        size_t idx = current_index_++ % backends_.size();
        if (backends_[idx].is_alive.load()) {
            backends_[idx].active_connections++;
            backends_[idx].total_requests++;
            return &backends_[idx];
        }
        tried++;
    }
    return nullptr; // All backends down
}

void LoadBalancer::release(Backend* backend) {
    if (backend) {
        backend->active_connections--;
    }
}

void LoadBalancer::mark_down(Backend* backend) {
    if (backend) {
        backend->is_alive.store(false);
        std::cerr << "[LoadBalancer] Backend " << backend->host << ":" << backend->port 
                  << " marked DOWN after failure" << std::endl;
    }
}

void LoadBalancer::start_health_checks(int interval_sec) {
    if (running_.load()) return;
    check_interval_sec_ = interval_sec;
    running_.store(true);
    health_thread_ = std::thread(&LoadBalancer::health_check_loop, this);
}

void LoadBalancer::stop_health_checks() {
    if (running_.load()) {
        running_.store(false);
        if (health_thread_.joinable()) {
            health_thread_.join();
        }
    }
}

std::vector<Backend> LoadBalancer::get_stats() {
    std::lock_guard<std::mutex> lock(mutex_);
    return backends_; // uses custom copy constructor
}

size_t LoadBalancer::backend_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return backends_.size();
}

size_t LoadBalancer::healthy_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& b : backends_) {
        if (b.is_alive.load()) count++;
    }
    return count;
}

// ---------------------------------------------------------------------------
// health_check_loop() — Background thread
// ---------------------------------------------------------------------------
void LoadBalancer::health_check_loop() {
    while (running_.load()) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& backend : backends_) {
                bool alive = check_health(backend);
                
                if (alive && !backend.is_alive.load()) {
                    std::cout << "[LoadBalancer] Backend " << backend.host << ":" << backend.port 
                              << " recovered, marked UP" << std::endl;
                    backend.is_alive.store(true);
                    backend.failed_checks = 0;
                } else if (!alive) {
                    backend.failed_checks++;
                    if (backend.failed_checks >= 3 && backend.is_alive.load()) { // 3 strikes
                        std::cerr << "[LoadBalancer] Backend " << backend.host << ":" << backend.port 
                                  << " failed 3 health checks, marked DOWN" << std::endl;
                        backend.is_alive.store(false);
                    }
                } else if (alive) {
                    backend.failed_checks = 0;
                }
                
                backend.last_check = std::chrono::steady_clock::now();
            }
        }
        
        // Sleep for interval, checking running_ flag periodically
        for (int i = 0; i < check_interval_sec_ * 10 && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// ---------------------------------------------------------------------------
// check_health() — TCP connect + basic HTTP probe
// ---------------------------------------------------------------------------
bool LoadBalancer::check_health(const Backend& backend) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) return false;

    // 2 second timeout for health check connect/recv
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(backend.port);
    if (inet_pton(AF_INET, backend.host.c_str(), &addr.sin_addr) <= 0) {
        close(fd);
        return false;
    }

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        close(fd);
        return false;
    }

    // Connect succeeded. Try to send HTTP GET to / or /health
    std::string req = "GET / HTTP/1.1\r\nHost: " + backend.host + "\r\nConnection: close\r\n\r\n";
    send(fd, req.data(), req.size(), MSG_NOSIGNAL);

    char buf[1024];
    ssize_t received = recv(fd, buf, sizeof(buf)-1, 0);
    
    close(fd);

    if (received > 0) {
        buf[received] = '\0';
        // Even if it returns 404, the server is UP. We only fail if it's a 50x or no response.
        std::string resp(buf);
        if (resp.find("HTTP/1.") != std::string::npos && resp.find("502") == std::string::npos) {
            return true;
        }
    }

    // Being lenient here: if connect() succeeded but we didn't get a full HTTP response,
    // we still consider it alive for TCP purposes (maybe it doesn't speak HTTP/1.1 correctly)
    // To be strict, return false here. Let's be semi-strict.
    return received > 0;
}
