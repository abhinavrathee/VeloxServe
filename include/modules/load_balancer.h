#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>

// Represents a single backend server in the load balancer pool
struct Backend {
    std::string host;
    int port;
    std::atomic<bool> is_alive{true};
    std::atomic<int> active_connections{0};
    std::atomic<int> total_requests{0};
    int failed_checks = 0;
    std::chrono::steady_clock::time_point last_check;
    
    // For copying in get_stats() 
    Backend(const std::string& h, int p) : host(h), port(p) {}
    Backend(const Backend& other) 
        : host(other.host), port(other.port), 
          is_alive(other.is_alive.load()), 
          active_connections(other.active_connections.load()),
          total_requests(other.total_requests.load()),
          failed_checks(other.failed_checks),
          last_check(other.last_check) {}
};

// Manages a pool of backends, distributes load round-robin, and runs health checks.
class LoadBalancer {
public:
    LoadBalancer() = default;
    ~LoadBalancer();

    // No copy
    LoadBalancer(const LoadBalancer&) = delete;
    LoadBalancer& operator=(const LoadBalancer&) = delete;

    void add_backend(const std::string& host, int port);

    // Get next healthy backend (round-robin). Returns nullptr if all down.
    // Automatically increments active_connections on the returned backend.
    Backend* get_next();

    // Decrement active_connections when proxy request completes
    void release(Backend* backend);

    // Mark a backend as down (e.g., if proxy connection fails)
    void mark_down(Backend* backend);

    // Start background health checking thread
    void start_health_checks(int interval_sec = 5);
    
    // Stop background thread
    void stop_health_checks();

    std::vector<Backend> get_stats();
    size_t backend_count() const;
    size_t healthy_count() const;

private:
    void health_check_loop();
    bool check_health(const Backend& backend);

    std::vector<Backend> backends_;
    mutable std::mutex mutex_;
    std::atomic<int> current_index_{0};

    std::thread health_thread_;
    std::atomic<bool> running_{false};
    int check_interval_sec_ = 5;
};
