#pragma once

#include <sys/epoll.h>
#include <vector>
#include <cstdint>

// RAII wrapper around Linux epoll — the high-performance I/O multiplexing mechanism.
// Monitors multiple file descriptors and notifies us when any has data ready.
// O(1) per event vs O(n) for select/poll.
class EpollWrapper {
public:
    EpollWrapper();
    ~EpollWrapper();

    // No copy — epoll fd is unique
    EpollWrapper(const EpollWrapper&) = delete;
    EpollWrapper& operator=(const EpollWrapper&) = delete;

    // Add fd to epoll monitoring with specified events (e.g., EPOLLIN | EPOLLET)
    bool add(int fd, uint32_t events);

    // Change which events we monitor for an fd
    bool modify(int fd, uint32_t events);

    // Stop monitoring an fd
    bool remove(int fd);

    // Block until events are ready. Returns number of ready events, -1 on error.
    // timeout_ms = -1 means block forever, 0 means return immediately.
    int wait(int timeout_ms = -1);

    // Access event at index after wait() returns
    const struct epoll_event& get_event(int index) const;

    // Helper: set a file descriptor to non-blocking mode
    static bool set_non_blocking(int fd);

private:
    int epoll_fd_;                              // epoll instance fd
    static constexpr int MAX_EVENTS = 1024;     // max events per wait() call
    std::vector<struct epoll_event> events_;     // pre-allocated buffer for results
};
