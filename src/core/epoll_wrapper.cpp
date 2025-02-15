#include "core/epoll_wrapper.h"
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

EpollWrapper::EpollWrapper() {
    // EPOLL_CLOEXEC: close epoll fd on exec() — prevents fd leak to child processes
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1) {
        throw std::runtime_error(
            std::string("epoll_create1 failed: ") + strerror(errno));
    }
    events_.resize(MAX_EVENTS);
}

EpollWrapper::~EpollWrapper() {
    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

bool EpollWrapper::add(int fd, uint32_t events) {
    struct epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        if (errno == EEXIST) {
            // fd already monitored — try modify instead
            return modify(fd, events);
        }
        std::cerr << "[epoll] Failed to add fd " << fd
                  << ": " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool EpollWrapper::modify(int fd, uint32_t events) {
    struct epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
        if (errno == ENOENT) {
            // fd not yet monitored — try add instead
            return add(fd, events);
        }
        std::cerr << "[epoll] Failed to modify fd " << fd
                  << ": " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool EpollWrapper::remove(int fd) {
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        // EBADF/ENOENT = fd already closed or not monitored — not a real error
        if (errno != EBADF && errno != ENOENT) {
            std::cerr << "[epoll] Failed to remove fd " << fd
                      << ": " << strerror(errno) << std::endl;
        }
        return false;
    }
    return true;
}

int EpollWrapper::wait(int timeout_ms) {
    int n = epoll_wait(epoll_fd_, events_.data(), MAX_EVENTS, timeout_ms);
    if (n == -1) {
        // EINTR = interrupted by signal (normal during shutdown) — return 0
        if (errno == EINTR) return 0;
        std::cerr << "[epoll] epoll_wait failed: " << strerror(errno) << std::endl;
        return -1;
    }
    return n;
}

const struct epoll_event& EpollWrapper::get_event(int index) const {
    return events_[index];
}

bool EpollWrapper::set_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "[epoll] fcntl F_GETFL failed for fd " << fd
                  << ": " << strerror(errno) << std::endl;
        return false;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "[epoll] fcntl F_SETFL failed for fd " << fd
                  << ": " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}
