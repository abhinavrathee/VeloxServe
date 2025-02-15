#include "core/thread_pool.h"
#include <iostream>
#include <stdexcept>

ThreadPool::ThreadPool(size_t num_threads) {
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;  // fallback if hw detection fails
    }

    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::worker_loop, this);
    }

    std::cout << "[VeloxServe] Thread pool started with "
              << num_threads << " workers" << std::endl;
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() {
    if (!shutdown_.load()) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            shutdown_.store(true);
        }
        // Wake ALL sleeping workers so they can see shutdown flag and exit
        condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }
}

void ThreadPool::enqueue(Task task) {
    if (shutdown_.load()) {
        throw std::runtime_error("Cannot enqueue on stopped ThreadPool");
    }
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        tasks_.push(std::move(task));
    }
    // Wake ONE sleeping worker to pick up the task
    condition_.notify_one();
}

size_t ThreadPool::queue_size() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

void ThreadPool::worker_loop() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Sleep until: shutdown requested OR a task is available
            condition_.wait(lock, [this] {
                return shutdown_.load() || !tasks_.empty();
            });

            // Exit if shutting down AND no remaining tasks
            if (shutdown_.load() && tasks_.empty()) {
                return;
            }

            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
            }
        }
        // Execute task OUTSIDE the lock — critical for performance
        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "[ThreadPool] Worker caught exception: "
                          << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[ThreadPool] Worker caught unknown exception"
                          << std::endl;
            }
        }
    }
}
