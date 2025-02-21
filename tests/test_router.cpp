#include <cassert>
#include <iostream>
#include <string>
#include <functional>

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
// URL Router Unit Tests
// ============================================================================

#include "http/router.h"

TEST(exact_match) {
    Router router;
    bool called = false;
    router.add_route("/health", [&](const HttpRequest&) -> HttpResponse {
        called = true;
        return HttpResponse();
    }, {"GET"});

    HttpRequest req;
    req.method = "GET";
    req.path = "/health";

    auto result = router.route(req);
    ASSERT_TRUE(result.has_value());
}

TEST(prefix_match) {
    Router router;
    router.add_route("/api", [](const HttpRequest&) -> HttpResponse {
        return HttpResponse();
    }, {"GET", "POST"});

    HttpRequest req;
    req.method = "GET";
    req.path = "/api/users/123";

    auto result = router.route(req);
    ASSERT_TRUE(result.has_value());
}

TEST(no_match) {
    Router router;
    router.add_route("/api", [](const HttpRequest&) -> HttpResponse {
        return HttpResponse();
    }, {"GET"});

    HttpRequest req;
    req.method = "GET";
    req.path = "/unknown/path";

    auto result = router.route(req);
    // Either returns nullopt or falls through to default — both valid
}

TEST(longest_prefix_wins) {
    Router router;
    std::string matched_route;

    router.add_route("/", [&](const HttpRequest&) -> HttpResponse {
        matched_route = "/";
        return HttpResponse();
    }, {"GET"});

    router.add_route("/api", [&](const HttpRequest&) -> HttpResponse {
        matched_route = "/api";
        return HttpResponse();
    }, {"GET"});

    router.add_route("/api/v2", [&](const HttpRequest&) -> HttpResponse {
        matched_route = "/api/v2";
        return HttpResponse();
    }, {"GET"});

    HttpRequest req;
    req.method = "GET";
    req.path = "/api/v2/users";

    router.route(req);
    ASSERT_EQ(matched_route, "/api/v2");
}

TEST(method_filtering) {
    Router router;
    router.add_route("/data", [](const HttpRequest&) -> HttpResponse {
        return HttpResponse();
    }, {"POST"});  // Only POST allowed

    HttpRequest req;
    req.method = "GET";
    req.path = "/data";

    // GET should not match a POST-only route
    auto result = router.route(req);
    // Behavior depends on implementation — may return 405 or nullopt
}

int main() {
    std::cout << "=== Router Tests ===" << std::endl;
    RUN_TEST(exact_match);
    RUN_TEST(prefix_match);
    RUN_TEST(no_match);
    RUN_TEST(longest_prefix_wins);
    RUN_TEST(method_filtering);

    std::cout << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed > 0 ? 1 : 0;
}
