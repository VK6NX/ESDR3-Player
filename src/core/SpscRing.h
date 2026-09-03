// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <atomic>
#include <cstddef>
#include <cstring>
#include <vector>

namespace esdr3 {

template <typename T>
class SpscRing {
public:
    explicit SpscRing(size_t capacity)
    {
        size_t cap = 1;
        while (cap < capacity) cap <<= 1;
        m_buf.resize(cap);
        m_mask = cap - 1;
    }

    size_t capacity() const { return m_buf.size(); }

    size_t available() const
    {
        return m_head.load(std::memory_order_acquire) - m_tail.load(std::memory_order_acquire);
    }

    size_t space() const { return capacity() - available(); }

    size_t write(const T* src, size_t n)
    {
        const size_t head = m_head.load(std::memory_order_relaxed);
        const size_t tail = m_tail.load(std::memory_order_acquire);
        const size_t free = capacity() - (head - tail);
        if (n > free) n = free;
        for (size_t i = 0; i < n; ++i)
            m_buf[(head + i) & m_mask] = src[i];
        m_head.store(head + n, std::memory_order_release);
        return n;
    }

    void requestFlush(size_t prefill = 0)
    {
        m_prefill.store(prefill, std::memory_order_relaxed);
        m_flush.store(true, std::memory_order_release);
    }

    size_t read(T* dst, size_t n)
    {
        if (m_flush.exchange(false, std::memory_order_acq_rel)) {
            m_tail.store(m_head.load(std::memory_order_acquire), std::memory_order_release);
            m_holding = m_prefill.load(std::memory_order_relaxed) > 0;
            return 0;
        }
        const size_t tail = m_tail.load(std::memory_order_relaxed);
        const size_t head = m_head.load(std::memory_order_acquire);
        if (m_holding) {
            if (head - tail < m_prefill.load(std::memory_order_relaxed)) return 0;
            m_holding = false;
        }
        const size_t avail = head - tail;
        if (n > avail) n = avail;
        for (size_t i = 0; i < n; ++i)
            dst[i] = m_buf[(tail + i) & m_mask];
        m_tail.store(tail + n, std::memory_order_release);
        return n;
    }

    void clear()
    {
        m_head.store(0);
        m_tail.store(0);
    }

private:
    std::vector<T> m_buf;
    size_t m_mask = 0;
    std::atomic<size_t> m_head{0};
    std::atomic<size_t> m_tail{0};
    std::atomic<bool> m_flush{false};
    std::atomic<size_t> m_prefill{0};
    bool m_holding = false;
};

using AudioRing = SpscRing<float>;

}
