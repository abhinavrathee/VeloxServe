#include "middleware/rate_limiter.h"

RateLimiter::RateLimiter(double rate, double burst, bool enabled)
    : rate_(rate), burst_(burst), enabled_(enabled)
{
    last_cleanup_ = std::chrono::steady_clock::now();
}

bool RateLimiter::is_allowed(const std::string& client_ip) {
    if (!enabled_) return true;

    std::lock_guard<std::mutex> lock(mutex_);
    total_requests_++;

    auto now = std::chrono::steady_clock::now();
    auto it = buckets_.find(client_ip);

    if (it == buckets_.end()) {
        // First time seeing this IP: create full bucket
        buckets_[client_ip] = {burst_ - 1.0, now};
        return true;
    }

    TokenBucket& bucket = it->second;

    // Calculate tokens to add based on elapsed time since last refill
    std::chrono::duration<double> elapsed = now - bucket.last_refill;
    double tokens_to_add = elapsed.count() * rate_;

    // Refill, capped at burst capacity
    bucket.tokens = std::min(burst_, bucket.tokens + tokens_to_add);
    bucket.last_refill = now;

    // Consume token if available
    if (bucket.tokens >= 1.0) {
        bucket.tokens -= 1.0;
        return true;
    } else {
        blocked_requests_++;
        return false;
    }
}

void RateLimiter::get_stats(size_t& total, size_t& blocked, size_t& active_ips) const {
    std::lock_guard<std::mutex> lock(mutex_);
    total = total_requests_;
    blocked = blocked_requests_;
    active_ips = buckets_.size();
}

void RateLimiter::cleanup_expired(int max_age_seconds) {
    if (!enabled_) return;

    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();

    // Only clean up periodically to avoid overhead
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_cleanup_).count() < 60) {
        return;
    }

    for (auto it = buckets_.begin(); it != buckets_.end(); ) {
        if (std::chrono::duration_cast<std::chrono::seconds>(now - it->second.last_refill).count() > max_age_seconds) {
            it = buckets_.erase(it);
        } else {
            ++it;
        }
    }
    last_cleanup_ = now;
}
