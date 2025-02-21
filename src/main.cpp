#include "core/server.h"
#include "config/config_parser.h"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <cstring>

#define VELOXSERVE_VERSION "1.0.0"

// Global pointer for signal handler
static Server* g_server = nullptr;

void signal_handler(int sig) {
    (void)sig;
    if (g_server) g_server->stop();
}

static void print_version() {
    std::cout << "VeloxServe v" << VELOXSERVE_VERSION << std::endl;
    std::cout << "A high-performance C++17 HTTP server & reverse proxy" << std::endl;
}

static void print_help(const char* program_name) {
    print_version();
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  " << program_name << " [options] [config_file]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help       Display this help message and exit" << std::endl;
    std::cout << "  -v, --version    Display version information and exit" << std::endl;
    std::cout << "  -t, --test       Test configuration file syntax and exit" << std::endl;
    std::cout << "  -p, --port PORT  Override listen port (default: 8080)" << std::endl;
    std::cout << std::endl;
    std::cout << "Arguments:" << std::endl;
    std::cout << "  config_file      Path to NGINX-style configuration file" << std::endl;
    std::cout << "                   If omitted, server starts with defaults on port 8080" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " veloxserve.conf" << std::endl;
    std::cout << "  " << program_name << " -t veloxserve.conf" << std::endl;
    std::cout << "  " << program_name << " -p 9090" << std::endl;
    std::cout << std::endl;
    std::cout << "Signals:" << std::endl;
    std::cout << "  SIGINT (Ctrl+C)  Graceful shutdown — drains active connections" << std::endl;
    std::cout << "  SIGTERM          Graceful shutdown — used by Docker/systemd" << std::endl;
    std::cout << std::endl;
    std::cout << "Endpoints:" << std::endl;
    std::cout << "  /health          Returns 200 OK if server is operational" << std::endl;
    std::cout << "  /metrics         Returns Prometheus-format server telemetry" << std::endl;
}

int main(int argc, char* argv[]) {
    // Register signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // CLI argument parsing
    std::string config_file;
    int port_override = -1;
    bool test_config = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            print_version();
            return 0;
        }
        if (arg == "-t" || arg == "--test") {
            test_config = true;
            continue;
        }
        if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port_override = std::atoi(argv[++i]);
            continue;
        }
        // Assume anything else is the config file path
        config_file = arg;
    }

    try {
        if (!config_file.empty()) {
            // ---- Config file mode ----
            std::cout << "[VeloxServe] Loading config: " << config_file << std::endl;

            ConfigParser parser;
            auto configs = parser.parse_file(config_file);
            auto upstreams = parser.upstreams();

            if (test_config) {
                std::cout << "[VeloxServe] Configuration syntax is OK" << std::endl;
                std::cout << "[VeloxServe] " << configs.size() << " server block(s) parsed" << std::endl;
                for (const auto& cfg : configs) {
                    std::cout << "[VeloxServe]   -> " << cfg.server_name
                              << ":" << cfg.port
                              << " (" << cfg.locations.size() << " locations)" << std::endl;
                }
                return 0;
            }

            // Apply port override if specified
            if (port_override > 0) {
                configs[0].port = port_override;
            }

            // Use first server block
            Server server(configs[0], upstreams);
            g_server = &server;
            server.start();
        } else {
            // ---- Default mode (no config) ----
            int port = (port_override > 0) ? port_override : 8080;
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

    std::cout << "[VeloxServe] Shutdown complete." << std::endl;
    return 0;
}
