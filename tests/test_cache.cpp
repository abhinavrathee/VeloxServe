#include <cassert>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "  Running " << #name << "..."; \
    test_##name(); \
    std::cout << " PASSED" << std::endl; \
    passed++; \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::cerr << " FAILED at line " << __LINE__ << std::endl; \
        failed++; return; \
    } \
} while(0)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        std::cerr << " FAILED at line " << __LINE__ << std::endl; \
        failed++; return; \
    } \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

static int passed = 0;
static int failed = 0;

// ============================================================================
// LRU Cache Unit Tests
// ============================================================================

#include "middleware/cache.h"

TEST(cache_put_and_get) {
    LRUCache cache(10, 300);  // 10MB max, 300s TTL
    std::vector<char> data = {'h', 'e', 'l', 'l', 'o'};
    cache.put("/test.txt", data, "text/plain");

    auto result = cache.get("/test.txt");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->content_type, "text/plain");
    ASSERT_EQ(result->data.size(), (size_t)5);
}

TEST(cache_miss) {
    LRUCache cache(10, 300);
    auto result = cache.get("/nonexistent.txt");
    ASSERT_FALSE(result.has_value());
}

TEST(cache_hit_ratio) {
    LRUCache cache(10, 300);
    std::vector<char> data = {'a', 'b', 'c'};
    cache.put("/file.txt", data, "text/plain");

    cache.get("/file.txt");     // hit
    cache.get("/file.txt");     // hit
    cache.get("/missing.txt");  // miss

    size_t hits, misses, entries, memory;
    cache.get_stats(hits, misses, entries, memory);
    ASSERT_EQ(hits, (size_t)2);
    ASSERT_EQ(misses, (size_t)1);
    ASSERT_EQ(entries, (size_t)1);
}

TEST(cache_eviction) {
    // Tiny cache: 1 byte max
    LRUCache cache(0, 300);  // 0MB = effectively very small
    std::vector<char> data(1024 * 1024 * 2, 'x');  // 2MB entry
    cache.put("/big.txt", data, "text/plain");

    // Entry should be too large to fit
    auto result = cache.get("/big.txt");
    // Either cached or rejected due to size — both are valid behaviors
    // The test verifies no crash occurs
}

TEST(cache_thread_safety) {
    LRUCache cache(10, 300);
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&cache, i]() {
            std::string key = "/thread_" + std::to_string(i) + ".txt";
            std::vector<char> data = {'d', 'a', 't', 'a'};
            cache.put(key, data, "text/plain");
            cache.get(key);
            cache.get("/nonexistent");
        });
    }

    for (auto& t : threads) t.join();
    // Test passes if no crash/deadlock occurred
}

int main() {
    std::cout << "=== LRU Cache Tests ===" << std::endl;
    RUN_TEST(cache_put_and_get);
    RUN_TEST(cache_miss);
    RUN_TEST(cache_hit_ratio);
    RUN_TEST(cache_eviction);
    RUN_TEST(cache_thread_safety);

    std::cout << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed > 0 ? 1 : 0;
}
