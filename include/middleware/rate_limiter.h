#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>

struct TokenBucket {
    double tokens;
    std::chrono::steady_clock::time_point last_refill;
};

// Token Bucket algorithm for rate limiting per client IP.
// Limits requests from a single IP to 'rate' per second, allowing bursts up to 'burst'.
class RateLimiter {
public:
    RateLimiter(double rate = 100.0, double burst = 200.0, bool enabled = true);

    // Checks if the IP has tokens available. If yes, consumes 1 and returns true.
    bool is_allowed(const std::string& client_ip);

    void get_stats(size_t& total, size_t& blocked, size_t& active_ips) const;
    void cleanup_expired(int max_age_seconds = 3600);

private:
    double rate_;
    double burst_;
    bool enabled_;

    std::unordered_map<std::string, TokenBucket> buckets_;
    mutable std::mutex mutex_;

    size_t total_requests_ = 0;
    size_t blocked_requests_ = 0;
    std::chrono::steady_clock::time_point last_cleanup_;
};
