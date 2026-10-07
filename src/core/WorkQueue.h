#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

namespace mc {

// Blocking multi-producer / multi-consumer queue for handing work between threads.
// push/pop take a mutex; fine for per-section granularity (not per block).
template <typename T> class WorkQueue {
public:
    void push(T item) {
        {
            std::lock_guard lock(m_mutex);
            m_items.push_back(std::move(item));
        }
        m_ready.notify_one();
    }

    // Blocks until an item arrives or close() is called (then returns nullopt).
    std::optional<T> popWait() {
        std::unique_lock lock(m_mutex);
        m_ready.wait(lock, [&] { return m_closed || !m_items.empty(); });
        if (m_items.empty()) return std::nullopt;
        T item = std::move(m_items.front());
        m_items.pop_front();
        return item;
    }

    std::optional<T> tryPop() {
        std::lock_guard lock(m_mutex);
        if (m_items.empty()) return std::nullopt;
        T item = std::move(m_items.front());
        m_items.pop_front();
        return item;
    }

    // Wakes all waiters; popWait returns nullopt once the queue is empty.
    // discard = also drop queued items (fast shutdown).
    void close(bool discard = false) {
        {
            std::lock_guard lock(m_mutex);
            m_closed = true;
            if (discard) m_items.clear();
        }
        m_ready.notify_all();
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_ready;
    std::deque<T> m_items;
    bool m_closed = false;
};

} // namespace mc
