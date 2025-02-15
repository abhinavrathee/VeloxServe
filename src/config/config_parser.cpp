#include "config/config_parser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>
#include <algorithm>

// ---------------------------------------------------------------------------
// Token helpers
// ---------------------------------------------------------------------------
std::string ConfigParser::Token::type_name() const {
    switch (type) {
        case TokenType::WORD:      return "WORD";
        case TokenType::LBRACE:    return "'{'";
        case TokenType::RBRACE:    return "'}'";
        case TokenType::SEMICOLON: return "';'";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------------------
// parse_file() — Read file, then parse
// ---------------------------------------------------------------------------
std::vector<ServerConfig> ConfigParser::parse_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + filename);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return parse_string(content);
}

// ---------------------------------------------------------------------------
// parse_string() — Tokenize, then recursive descent parse
// ---------------------------------------------------------------------------
std::vector<ServerConfig> ConfigParser::parse_string(const std::string& input) {
    tokens_ = tokenize(input);
    pos_ = 0;
    upstreams_.clear();
    return parse_config();
}

// ===========================================================================
// STAGE 1: TOKENIZER
// Scans raw text into tokens: WORD, {, }, ;
// Skips whitespace and # comments
// ===========================================================================
std::vector<ConfigParser::Token> ConfigParser::tokenize(const std::string& input) {
    std::vector<Token> tokens;
    size_t i = 0;
    int line = 1;

    while (i < input.size()) {
        char c = input[i];

        // Track line numbers for error messages
        if (c == '\n') {
            line++;
            i++;
            continue;
        }

        // Skip whitespace
        if (std::isspace(c)) {
            i++;
            continue;
        }

        // Skip # comments until end of line
        if (c == '#') {
            while (i < input.size() && input[i] != '\n') i++;
            continue;
        }

        // Single-character tokens
        if (c == '{') {
            tokens.push_back({TokenType::LBRACE, "{", line});
            i++;
            continue;
        }
        if (c == '}') {
            tokens.push_back({TokenType::RBRACE, "}", line});
            i++;
            continue;
        }
        if (c == ';') {
            tokens.push_back({TokenType::SEMICOLON, ";", line});
            i++;
            continue;
        }

        // Word: everything else until whitespace or special char
        std::string word;
        while (i < input.size() && !std::isspace(input[i]) &&
               input[i] != '{' && input[i] != '}' &&
               input[i] != ';' && input[i] != '#') {
            word += input[i];
            i++;
        }
        if (!word.empty()) {
            tokens.push_back({TokenType::WORD, word, line});
        }
    }

    return tokens;
}

// ===========================================================================
// STAGE 2: RECURSIVE DESCENT PARSER
// ===========================================================================

// ---- Parser helpers ----

ConfigParser::Token& ConfigParser::current() {
    if (pos_ >= tokens_.size()) {
        static Token eof{TokenType::WORD, "<EOF>", -1};
        return eof;
    }
    return tokens_[pos_];
}

ConfigParser::Token ConfigParser::consume(TokenType expected) {
    if (at_end()) {
        error("Unexpected end of file, expected " + Token{expected, "", 0}.type_name());
    }
    if (current().type != expected) {
        error("Expected " + Token{expected, "", 0}.type_name() +
              " but got " + current().type_name() + " '" + current().value + "'");
    }
    return tokens_[pos_++];
}

bool ConfigParser::check(TokenType type) const {
    return !at_end() && tokens_[pos_].type == type;
}

bool ConfigParser::check_word(const std::string& value) const {
    return check(TokenType::WORD) && tokens_[pos_].value == value;
}

bool ConfigParser::at_end() const {
    return pos_ >= tokens_.size();
}

[[noreturn]] void ConfigParser::error(const std::string& message) {
    int line = at_end() ? -1 : current().line;
    throw ConfigError(line, message);
}

// ---- Top-level: parse server and upstream blocks ----

std::vector<ServerConfig> ConfigParser::parse_config() {
    std::vector<ServerConfig> configs;

    while (!at_end()) {
        if (check_word("server")) {
            configs.push_back(parse_server_block());
        } else if (check_word("upstream")) {
            auto upstream = parse_upstream_block();
            upstreams_[upstream.name] = upstream;
        } else {
            error("Expected 'server' or 'upstream' block, got '" + current().value + "'");
        }
    }

    if (configs.empty()) {
        throw std::runtime_error("Config file must contain at least one server block");
    }

    return configs;
}

// ---- Parse: server { ... } ----

ServerConfig ConfigParser::parse_server_block() {
    consume(TokenType::WORD);  // "server"
    consume(TokenType::LBRACE);

    ServerConfig config;

    while (!check(TokenType::RBRACE)) {
        if (at_end()) error("Unclosed server block — missing '}'");

        if (check_word("location")) {
            config.locations.push_back(parse_location_block());
        } else {
            parse_server_directive(config);
        }
    }

    consume(TokenType::RBRACE);
    return config;
}

// ---- Parse: location /path { ... } ----

LocationConfig ConfigParser::parse_location_block() {
    consume(TokenType::WORD);  // "location"

    LocationConfig loc;
    loc.path = consume(TokenType::WORD).value;  // the path like "/api"

    consume(TokenType::LBRACE);

    while (!check(TokenType::RBRACE)) {
        if (at_end()) error("Unclosed location block — missing '}'");
        parse_location_directive(loc);
    }

    consume(TokenType::RBRACE);
    return loc;
}

// ---- Parse: upstream name { server ...; server ...; } ----

UpstreamConfig ConfigParser::parse_upstream_block() {
    consume(TokenType::WORD);  // "upstream"

    UpstreamConfig upstream;
    upstream.name = consume(TokenType::WORD).value;

    consume(TokenType::LBRACE);

    while (!check(TokenType::RBRACE)) {
        if (at_end()) error("Unclosed upstream block — missing '}'");

        std::string directive = consume(TokenType::WORD).value;

        if (directive == "server") {
            std::string addr = consume(TokenType::WORD).value;
            // Parse "127.0.0.1:3001" → host + port
            size_t colon = addr.find_last_of(':');
            if (colon != std::string::npos) {
                std::string host = addr.substr(0, colon);
                int port = std::stoi(addr.substr(colon + 1));
                upstream.servers.push_back({host, port});
            } else {
                upstream.servers.push_back({addr, 80});
            }
            consume(TokenType::SEMICOLON);
        } else if (directive == "health_check_interval") {
            upstream.health_check_interval = std::stoi(consume(TokenType::WORD).value);
            consume(TokenType::SEMICOLON);
        } else {
            error("Unknown upstream directive: " + directive);
        }
    }

    consume(TokenType::RBRACE);
    return upstream;
}

// ---- Parse individual server directives ----

void ConfigParser::parse_server_directive(ServerConfig& config) {
    std::string directive = consume(TokenType::WORD).value;

    if (directive == "listen") {
        std::string value = consume(TokenType::WORD).value;
        // Support "8080" or "0.0.0.0:8080"
        size_t colon = value.find(':');
        if (colon != std::string::npos) {
            config.host = value.substr(0, colon);
            config.port = std::stoi(value.substr(colon + 1));
        } else {
            config.port = std::stoi(value);
        }
        consume(TokenType::SEMICOLON);

    } else if (directive == "server_name") {
        config.server_name = consume(TokenType::WORD).value;
        consume(TokenType::SEMICOLON);

    } else if (directive == "root") {
        config.root = consume(TokenType::WORD).value;
        consume(TokenType::SEMICOLON);

    } else if (directive == "error_page") {
        int code = std::stoi(consume(TokenType::WORD).value);
        std::string path = consume(TokenType::WORD).value;
        config.error_pages[code] = path;
        consume(TokenType::SEMICOLON);

    } else if (directive == "client_max_body_size") {
        config.client_max_body_size = parse_size(consume(TokenType::WORD).value);
        consume(TokenType::SEMICOLON);

    } else if (directive == "rate_limit") {
        config.rate_limit = std::stoi(consume(TokenType::WORD).value);
        consume(TokenType::SEMICOLON);

    } else {
        error("Unknown server directive: " + directive);
    }
}

// ---- Parse individual location directives ----

void ConfigParser::parse_location_directive(LocationConfig& loc) {
    std::string directive = consume(TokenType::WORD).value;

    if (directive == "root") {
        loc.root = consume(TokenType::WORD).value;
        consume(TokenType::SEMICOLON);

    } else if (directive == "index") {
        loc.index_file = consume(TokenType::WORD).value;
        consume(TokenType::SEMICOLON);

    } else if (directive == "proxy_pass") {
        loc.proxy_pass = consume(TokenType::WORD).value;
        consume(TokenType::SEMICOLON);

    } else if (directive == "methods") {
        // Read all method words until semicolon
        while (check(TokenType::WORD)) {
            loc.methods.push_back(consume(TokenType::WORD).value);
        }
        consume(TokenType::SEMICOLON);

    } else if (directive == "autoindex") {
        std::string val = consume(TokenType::WORD).value;
        loc.autoindex = (val == "on" || val == "true" || val == "yes");
        consume(TokenType::SEMICOLON);

    } else {
        error("Unknown location directive: " + directive);
    }
}

// ---- Size parser: "10M" → bytes ----

size_t ConfigParser::parse_size(const std::string& str) {
    if (str.empty()) return 0;

    char suffix = str.back();
    std::string number_part = str;

    size_t multiplier = 1;
    if (suffix == 'K' || suffix == 'k') {
        multiplier = 1024;
        number_part.pop_back();
    } else if (suffix == 'M' || suffix == 'm') {
        multiplier = 1024 * 1024;
        number_part.pop_back();
    } else if (suffix == 'G' || suffix == 'g') {
        multiplier = 1024ULL * 1024 * 1024;
        number_part.pop_back();
    }

    return std::stoul(number_part) * multiplier;
}
