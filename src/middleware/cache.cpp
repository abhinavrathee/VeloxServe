#include "middleware/cache.h"
#include <iostream>

LRUCache::LRUCache(size_t max_size_bytes, int ttl_seconds)
    : max_size_bytes_(max_size_bytes), ttl_seconds_(ttl_seconds) {}

std::optional<CacheEntry> LRUCache::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(key);
    if (it == cache_.end()) {
        misses_++;
        return std::nullopt;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.first.created).count();
    
    // Check TTL expiration
    if (elapsed > ttl_seconds_) {
        // Evict expired entry
        current_size_ -= it->second.first.data.size();
        lru_list_.erase(it->second.second);
        cache_.erase(it);
        misses_++;
        return std::nullopt;
    }

    // Cache HIT
    hits_++;

    // Move key to front of LRU list
    lru_list_.erase(it->second.second);
    lru_list_.push_front(key);
    it->second.second = lru_list_.begin();

    // Update stats
    it->second.first.last_accessed = now;
    it->second.first.access_count++;

    return it->second.first;
}

void LRUCache::put(const std::string& key, const std::string& data, const std::string& content_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Skip if single item is larger than entire cache
    if (data.size() > max_size_bytes_) {
        return;
    }

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        // Update existing entry
        current_size_ -= it->second.first.data.size();
        lru_list_.erase(it->second.second);
        cache_.erase(it);
    }

    // Evict items if adding this new data exceeds memory limits
    while (!lru_list_.empty() && (current_size_ + data.size() > max_size_bytes_)) {
        std::string lru_key = lru_list_.back();
        auto lru_it = cache_.find(lru_key);
        if (lru_it != cache_.end()) {
            current_size_ -= lru_it->second.first.data.size();
            cache_.erase(lru_it);
        }
        lru_list_.pop_back();
    }

    // Insert new item at the front
    lru_list_.push_front(key);
    
    CacheEntry entry;
    entry.data = data;
    entry.content_type = content_type;
    entry.created = std::chrono::steady_clock::now();
    entry.last_accessed = entry.created;
    entry.access_count = 0;

    cache_[key] = {std::move(entry), lru_list_.begin()};
    current_size_ += data.size();
}

void LRUCache::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        current_size_ -= it->second.first.data.size();
        lru_list_.erase(it->second.second);
        cache_.erase(it);
    }
}

void LRUCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    lru_list_.clear();
    cache_.clear();
    current_size_ = 0;
}

double LRUCache::hit_ratio() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t total = hits_ + misses_;
    if (total == 0) return 0.0;
    return static_cast<double>(hits_) / total;
}

void LRUCache::get_stats(size_t& hits, size_t& misses, size_t& entries, size_t& memory) const {
    std::lock_guard<std::mutex> lock(mutex_);
    hits = hits_;
    misses = misses_;
    entries = cache_.size();
    memory = current_size_;
}
