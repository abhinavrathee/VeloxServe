#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <cstddef>

// Fixed-size pool of worker threads that execute tasks from a shared queue.
// Avoids the overhead of creating/destroying threads per request.
class ThreadPool {
public:
    using Task = std::function<void()>;

    // Create pool with num_threads workers. 0 = auto-detect from hardware.
    explicit ThreadPool(size_t num_threads = 0);
    ~ThreadPool();

    // No copy/move — pool owns threads
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Submit a task for execution by a worker thread
    void enqueue(Task task);

    // Graceful shutdown — finish current tasks, stop accepting new ones
    void shutdown();

    size_t queue_size() const;
    size_t thread_count() const { return workers_.size(); }
    bool is_shutdown() const { return shutdown_.load(); }

private:
    void worker_loop();

    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;

    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> shutdown_{false};
};
