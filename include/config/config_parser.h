#pragma once

#include "config/server_config.h"

#include <string>
#include <vector>
#include <map>
#include <stdexcept>

// NGINX-style configuration parser.
// 3-stage pipeline: file → tokenize → recursive descent parse → config structs.
class ConfigParser {
public:
    // Parse config file, return vector of ServerConfigs (supports multiple server blocks)
    std::vector<ServerConfig> parse_file(const std::string& filename);

    // Parse config from string (for testing)
    std::vector<ServerConfig> parse_string(const std::string& input);

    // Get parsed upstream blocks (for load balancer)
    const std::map<std::string, UpstreamConfig>& upstreams() const { return upstreams_; }

private:
    // ---- Stage 1: Tokenizer ----
    enum class TokenType { WORD, LBRACE, RBRACE, SEMICOLON };

    struct Token {
        TokenType type;
        std::string value;
        int line;  // for error messages

        std::string type_name() const;
    };

    std::vector<Token> tokenize(const std::string& input);

    // ---- Stage 2: Recursive Descent Parser ----
    std::vector<Token> tokens_;
    size_t pos_ = 0;
    std::map<std::string, UpstreamConfig> upstreams_;

    // Parser helpers
    Token& current();
    Token consume(TokenType expected);
    bool check(TokenType type) const;
    bool check_word(const std::string& value) const;
    bool at_end() const;
    [[noreturn]] void error(const std::string& message);

    // Parse rules
    std::vector<ServerConfig> parse_config();
    ServerConfig parse_server_block();
    LocationConfig parse_location_block();
    UpstreamConfig parse_upstream_block();
    void parse_server_directive(ServerConfig& config);
    void parse_location_directive(LocationConfig& loc);

    // Size parsing: "10M" → 10485760
    size_t parse_size(const std::string& str);
};

// Exception for config parse errors (includes line number)
class ConfigError : public std::runtime_error {
public:
    ConfigError(int line, const std::string& msg)
        : std::runtime_error("Config error (line " + std::to_string(line) + "): " + msg)
        , line_(line) {}
    int line() const { return line_; }
private:
    int line_;
};
