#include <cassert>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

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
// Token-Bucket Rate Limiter Unit Tests
// ============================================================================

#include "middleware/rate_limiter.h"

TEST(allow_within_limit) {
    RateLimiter limiter(10.0, 10.0, true);  // 10 req/sec, burst 10
    // First 10 should be allowed (burst capacity)
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(limiter.is_allowed("192.168.1.1"));
    }
}

TEST(block_over_limit) {
    RateLimiter limiter(5.0, 5.0, true);  // 5 req/sec, burst 5
    // Exhaust burst
    for (int i = 0; i < 5; ++i) {
        limiter.is_allowed("10.0.0.1");
    }
    // Next should be blocked
    ASSERT_FALSE(limiter.is_allowed("10.0.0.1"));
}

TEST(per_ip_isolation) {
    RateLimiter limiter(3.0, 3.0, true);  // 3 req/sec, burst 3
    // Exhaust IP A
    for (int i = 0; i < 3; ++i) {
        limiter.is_allowed("1.1.1.1");
    }
    ASSERT_FALSE(limiter.is_allowed("1.1.1.1"));

    // IP B should still be allowed (independent bucket)
    ASSERT_TRUE(limiter.is_allowed("2.2.2.2"));
}

TEST(disabled_limiter) {
    RateLimiter limiter(1.0, 1.0, false);  // disabled
    // Should always allow when disabled
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(limiter.is_allowed("10.0.0.1"));
    }
}

TEST(stats_tracking) {
    RateLimiter limiter(2.0, 2.0, true);
    limiter.is_allowed("1.1.1.1");  // allowed
    limiter.is_allowed("1.1.1.1");  // allowed
    limiter.is_allowed("1.1.1.1");  // blocked

    size_t total, blocked, active_ips;
    limiter.get_stats(total, blocked, active_ips);
    ASSERT_EQ(total, (size_t)3);
    ASSERT_EQ(blocked, (size_t)1);
    ASSERT_EQ(active_ips, (size_t)1);
}

TEST(token_refill) {
    RateLimiter limiter(100.0, 1.0, true);  // 100 req/sec, burst 1
    ASSERT_TRUE(limiter.is_allowed("5.5.5.5"));
    ASSERT_FALSE(limiter.is_allowed("5.5.5.5"));

    // Wait for refill (>10ms at 100 req/sec should yield 1+ token)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ASSERT_TRUE(limiter.is_allowed("5.5.5.5"));
}

int main() {
    std::cout << "=== Rate Limiter Tests ===" << std::endl;
    RUN_TEST(allow_within_limit);
    RUN_TEST(block_over_limit);
    RUN_TEST(per_ip_isolation);
    RUN_TEST(disabled_limiter);
    RUN_TEST(stats_tracking);
    RUN_TEST(token_refill);

    std::cout << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed > 0 ? 1 : 0;
}
