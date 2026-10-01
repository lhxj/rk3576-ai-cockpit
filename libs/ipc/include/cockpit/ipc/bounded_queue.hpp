#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace cockpit::ipc {

enum class QueueStatus { OK, FULL, EMPTY, TIMEOUT, CLOSED };

struct QueueStats {
    std::size_t size{0};
    std::uint64_t overflow{0};
};

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
        if (capacity == 0) throw std::invalid_argument("queue capacity must be positive");
    }
    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    QueueStatus try_push(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_) return QueueStatus::CLOSED;
        if (items_.size() == capacity_) { ++overflow_; return QueueStatus::FULL; }
        items_.push_back(std::move(value));
        not_empty_.notify_one();
        return QueueStatus::OK;
    }

    template <typename Rep, typename Period>
    QueueStatus push_for(T value, std::chrono::duration<Rep, Period> timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!not_full_.wait_for(lock, timeout, [this] { return closed_ || items_.size() < capacity_; })) {
            ++overflow_;
            return QueueStatus::TIMEOUT;
        }
        if (closed_) return QueueStatus::CLOSED;
        items_.push_back(std::move(value));
        not_empty_.notify_one();
        return QueueStatus::OK;
    }

    QueueStatus try_pop(T& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (items_.empty()) return closed_ ? QueueStatus::CLOSED : QueueStatus::EMPTY;
        out = std::move(items_.front());
        items_.pop_front();
        not_full_.notify_one();
        return QueueStatus::OK;
    }

    template <typename Rep, typename Period>
    QueueStatus pop_for(T& out, std::chrono::duration<Rep, Period> timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!not_empty_.wait_for(lock, timeout, [this] { return closed_ || !items_.empty(); }))
            return QueueStatus::TIMEOUT;
        if (items_.empty()) return QueueStatus::CLOSED;
        out = std::move(items_.front());
        items_.pop_front();
        not_full_.notify_one();
        return QueueStatus::OK;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }
    std::size_t capacity() const { return capacity_; }
    std::size_t size() const { return stats().size; }
    QueueStats stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {items_.size(), overflow_};
    }
    bool closed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::deque<T> items_;
    bool closed_{false};
    std::uint64_t overflow_{0};
};

}  // namespace cockpit::ipc
