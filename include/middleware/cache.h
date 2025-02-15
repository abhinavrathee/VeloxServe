#pragma once

#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <optional>

struct CacheEntry {
    std::string data;
    std::string content_type;
    std::chrono::steady_clock::time_point created;
    std::chrono::steady_clock::time_point last_accessed;
    size_t access_count = 0;
};

// Thread-safe LRU cache to store serving responses (typically static files in memory)
// Limits size based on memory bytes. Includes Time-To-Live (TTL) expiration.
class LRUCache {
public:
    LRUCache(size_t max_size_bytes = 100 * 1024 * 1024, int ttl_seconds = 300);

    // Get entry from cache (O(1)). Returns std::nullopt if miss or expired.
    std::optional<CacheEntry> get(const std::string& key);

    // Put or update entry in cache (O(1)). Evicts LRU if capacity exceeded.
    void put(const std::string& key, const std::string& data, const std::string& content_type);

    void remove(const std::string& key);
    void clear();

    double hit_ratio() const;
    void get_stats(size_t& hits, size_t& misses, size_t& entries, size_t& memory) const;

private:
    using CacheList = std::list<std::string>;
    using CacheMap = std::unordered_map<std::string, std::pair<CacheEntry, CacheList::iterator>>;

    CacheList lru_list_;
    CacheMap cache_;
    mutable std::mutex mutex_;

    size_t max_size_bytes_;
    int ttl_seconds_;
    size_t current_size_ = 0;

    mutable size_t hits_ = 0;
    mutable size_t misses_ = 0;
};
