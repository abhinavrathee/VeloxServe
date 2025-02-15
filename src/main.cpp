#include "core/server.h"
#include "config/config_parser.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

// Global pointer for signal handler
static Server* g_server = nullptr;

void signal_handler(int sig) {
    (void)sig;
    if (g_server) g_server->stop();
}

int main(int argc, char* argv[]) {
    // Register signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try {
        if (argc > 1) {
            // ---- Config file mode ----
            std::string config_file = argv[1];
            std::cout << "[VeloxServe] Loading config: " << config_file << std::endl;

            ConfigParser parser;
            auto configs = parser.parse_file(config_file);
            auto upstreams = parser.upstreams();

            // Use first server block
            Server server(configs[0], upstreams);
            g_server = &server;
            server.start();
        } else {
            // ---- Default mode (no config) ----
            int port = 8080;
            std::cout << "[VeloxServe] No config file specified, using defaults" << std::endl;

            Server server(port);
            g_server = &server;
            server.start();
        }
    } catch (const ConfigError& e) {
        std::cerr << "[VeloxServe] " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "[VeloxServe] Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
