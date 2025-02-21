#include <cassert>
#include <iostream>
#include <string>

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
// Load Balancer Unit Tests
// ============================================================================

#include "modules/load_balancer.h"

TEST(add_backends) {
    LoadBalancer lb;
    lb.add_backend("127.0.0.1", 3001);
    lb.add_backend("127.0.0.1", 3002);
    lb.add_backend("127.0.0.1", 3003);
    ASSERT_EQ(lb.backend_count(), (size_t)3);
    ASSERT_EQ(lb.healthy_count(), (size_t)3);
}

TEST(round_robin_distribution) {
    LoadBalancer lb;
    lb.add_backend("127.0.0.1", 3001);
    lb.add_backend("127.0.0.1", 3002);
    lb.add_backend("127.0.0.1", 3003);

    auto* b1 = lb.get_next();
    auto* b2 = lb.get_next();
    auto* b3 = lb.get_next();
    auto* b4 = lb.get_next();  // Should wrap around to first

    ASSERT_TRUE(b1 != nullptr);
    ASSERT_TRUE(b2 != nullptr);
    ASSERT_TRUE(b3 != nullptr);
    ASSERT_TRUE(b4 != nullptr);

    // Ports should cycle: 3001, 3002, 3003, 3001
    ASSERT_EQ(b1->port, 3001);
    ASSERT_EQ(b2->port, 3002);
    ASSERT_EQ(b3->port, 3003);
    ASSERT_EQ(b4->port, 3001);

    lb.release(b1);
    lb.release(b2);
    lb.release(b3);
    lb.release(b4);
}

TEST(skip_dead_backend) {
    LoadBalancer lb;
    lb.add_backend("127.0.0.1", 3001);
    lb.add_backend("127.0.0.1", 3002);
    lb.add_backend("127.0.0.1", 3003);

    // Mark backend 2 as down
    auto* b = lb.get_next();  // 3001
    lb.release(b);
    b = lb.get_next();        // 3002
    lb.mark_down(b);
    lb.release(b);

    // Next calls should skip 3002
    b = lb.get_next();
    ASSERT_TRUE(b != nullptr);
    ASSERT_TRUE(b->port != 3002);
    lb.release(b);
}

TEST(all_backends_down) {
    LoadBalancer lb;
    lb.add_backend("127.0.0.1", 3001);

    auto* b = lb.get_next();
    lb.mark_down(b);
    lb.release(b);

    auto* result = lb.get_next();
    ASSERT_TRUE(result == nullptr);  // No healthy backends
}

TEST(release_decrements_connections) {
    LoadBalancer lb;
    lb.add_backend("127.0.0.1", 3001);

    auto* b = lb.get_next();
    ASSERT_TRUE(b != nullptr);
    ASSERT_EQ(b->active_connections, 1);

    lb.release(b);
    ASSERT_EQ(b->active_connections, 0);
}

int main() {
    std::cout << "=== Load Balancer Tests ===" << std::endl;
    RUN_TEST(add_backends);
    RUN_TEST(round_robin_distribution);
    RUN_TEST(skip_dead_backend);
    RUN_TEST(all_backends_down);
    RUN_TEST(release_decrements_connections);

    std::cout << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed > 0 ? 1 : 0;
}
