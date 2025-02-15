#pragma once

#include <string>
#include <chrono>
#include <cstddef>

// Holds all state for one client TCP connection.
// Each accepted client gets its own Connection object.
struct Connection {
    int fd;                          // Socket file descriptor
    std::string read_buffer;         // Accumulates raw bytes received (may be partial HTTP)
    std::string write_buffer;        // Response bytes waiting to be sent
    bool keep_alive = false;         // Client wants connection reuse?
    std::chrono::steady_clock::time_point last_activity;  // For timeout cleanup
    size_t bytes_sent = 0;           // How many bytes of write_buffer sent so far
    bool is_writing = false;         // Currently in the middle of sending?

    explicit Connection(int socket_fd)
        : fd(socket_fd)
        , last_activity(std::chrono::steady_clock::now()) {}

    // Connections are unique to a socket — no copying
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = default;
    Connection& operator=(Connection&&) = default;
};
