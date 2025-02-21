#include <cassert>
#include <iostream>
#include <string>

// Minimal test framework (no GoogleTest dependency required)
#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "  Running " << #name << "..."; \
    test_##name(); \
    std::cout << " PASSED" << std::endl; \
    passed++; \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::cerr << " FAILED at line " << __LINE__ \
                  << ": " << (a) << " != " << (b) << std::endl; \
        failed++; return; \
    } \
} while(0)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        std::cerr << " FAILED at line " << __LINE__ \
                  << ": expression is false" << std::endl; \
        failed++; return; \
    } \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

static int passed = 0;
static int failed = 0;

// ============================================================================
// HTTP Parser Unit Tests
// ============================================================================

#include "http/http_parser.h"

TEST(parse_get_request) {
    HttpParser parser;
    std::string raw = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    parser.feed(raw.c_str(), raw.size());
    ASSERT_TRUE(parser.is_complete());

    auto req = parser.get_request();
    ASSERT_EQ(req.method, "GET");
    ASSERT_EQ(req.path, "/index.html");
    ASSERT_EQ(req.version, "HTTP/1.1");
}

TEST(parse_post_request_with_body) {
    HttpParser parser;
    std::string raw = "POST /api/data HTTP/1.1\r\n"
                      "Host: localhost\r\n"
                      "Content-Length: 11\r\n"
                      "\r\n"
                      "hello=world";
    parser.feed(raw.c_str(), raw.size());
    ASSERT_TRUE(parser.is_complete());

    auto req = parser.get_request();
    ASSERT_EQ(req.method, "POST");
    ASSERT_EQ(req.path, "/api/data");
    ASSERT_EQ(req.body, "hello=world");
}

TEST(parse_headers) {
    HttpParser parser;
    std::string raw = "GET / HTTP/1.1\r\n"
                      "Host: example.com\r\n"
                      "User-Agent: VeloxTest/1.0\r\n"
                      "Accept: text/html\r\n"
                      "\r\n";
    parser.feed(raw.c_str(), raw.size());
    ASSERT_TRUE(parser.is_complete());

    auto req = parser.get_request();
    ASSERT_EQ(req.get_header("Host"), "example.com");
    ASSERT_EQ(req.get_header("User-Agent"), "VeloxTest/1.0");
}

TEST(incomplete_request) {
    HttpParser parser;
    std::string partial = "GET /index.html HTTP/1.1\r\nHost: loc";
    parser.feed(partial.c_str(), partial.size());
    ASSERT_FALSE(parser.is_complete());
}

int main() {
    std::cout << "=== HTTP Parser Tests ===" << std::endl;
    RUN_TEST(parse_get_request);
    RUN_TEST(parse_post_request_with_body);
    RUN_TEST(parse_headers);
    RUN_TEST(incomplete_request);

    std::cout << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed > 0 ? 1 : 0;
}
