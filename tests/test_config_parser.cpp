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
// Config Parser Unit Tests
// ============================================================================

#include "config/config_parser.h"

TEST(parse_server_block) {
    std::string config_text =
        "server {\n"
        "    listen 9090;\n"
        "    server_name testhost;\n"
        "    root ./test_www;\n"
        "}\n";

    ConfigParser parser;
    auto configs = parser.parse_string(config_text);
    ASSERT_EQ(configs.size(), (size_t)1);
    ASSERT_EQ(configs[0].port, 9090);
    ASSERT_EQ(configs[0].server_name, "testhost");
    ASSERT_EQ(configs[0].root, "./test_www");
}

TEST(parse_location_block) {
    std::string config_text =
        "server {\n"
        "    listen 8080;\n"
        "    location / {\n"
        "        root ./www;\n"
        "        index index.html;\n"
        "        methods GET HEAD;\n"
        "    }\n"
        "}\n";

    ConfigParser parser;
    auto configs = parser.parse_string(config_text);
    ASSERT_EQ(configs[0].locations.size(), (size_t)1);
    ASSERT_EQ(configs[0].locations[0].path, "/");
    ASSERT_EQ(configs[0].locations[0].root, "./www");
    ASSERT_FALSE(configs[0].locations[0].is_proxy());
}

TEST(parse_proxy_location) {
    std::string config_text =
        "server {\n"
        "    listen 8080;\n"
        "    location /api {\n"
        "        proxy_pass http://127.0.0.1:3000;\n"
        "        methods GET POST;\n"
        "    }\n"
        "}\n";

    ConfigParser parser;
    auto configs = parser.parse_string(config_text);
    ASSERT_TRUE(configs[0].locations[0].is_proxy());
    ASSERT_EQ(configs[0].locations[0].proxy_pass, "http://127.0.0.1:3000");
}

TEST(parse_rate_limit) {
    std::string config_text =
        "server {\n"
        "    listen 8080;\n"
        "    rate_limit 50;\n"
        "}\n";

    ConfigParser parser;
    auto configs = parser.parse_string(config_text);
    ASSERT_EQ(configs[0].rate_limit, 50);
}

TEST(parse_error_pages) {
    std::string config_text =
        "server {\n"
        "    listen 8080;\n"
        "    error_page 404 /errors/404.html;\n"
        "    error_page 500 /errors/500.html;\n"
        "}\n";

    ConfigParser parser;
    auto configs = parser.parse_string(config_text);
    ASSERT_EQ(configs[0].error_pages[404], "/errors/404.html");
    ASSERT_EQ(configs[0].error_pages[500], "/errors/500.html");
}

int main() {
    std::cout << "=== Config Parser Tests ===" << std::endl;
    RUN_TEST(parse_server_block);
    RUN_TEST(parse_location_block);
    RUN_TEST(parse_proxy_location);
    RUN_TEST(parse_rate_limit);
    RUN_TEST(parse_error_pages);

    std::cout << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed > 0 ? 1 : 0;
}
