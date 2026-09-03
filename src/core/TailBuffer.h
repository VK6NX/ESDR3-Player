// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace esdr3 {

template <typename T>
class TailBuffer {
public:
    explicit TailBuffer(size_t capacity) : m_buf(capacity), m_cap(capacity) {}

    size_t capacity() const { return m_cap; }

    void push(const T* src, size_t n, uint64_t streamPos)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (n >= m_cap) {
            src += n - m_cap;
            n = m_cap;
        }
        for (size_t i = 0; i < n; ++i) {
            m_buf[m_write] = src[i];
            m_write = (m_write + 1) % m_cap;
        }
        m_written += n;
        m_streamPos = streamPos;
    }

    bool copyLast(T* dst, size_t n, uint64_t* streamPos) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (n > m_cap || m_written < n) return false;
        size_t start = (m_write + m_cap - n) % m_cap;
        for (size_t i = 0; i < n; ++i)
            dst[i] = m_buf[(start + i) % m_cap];
        if (streamPos) *streamPos = m_streamPos;
        return true;
    }

    uint64_t totalWritten() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_written;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::fill(m_buf.begin(), m_buf.end(), T{});
        m_write = 0;
        m_written = 0;
        m_streamPos = 0;
    }

private:
    mutable std::mutex m_mutex;
    std::vector<T> m_buf;
    size_t m_cap;
    size_t m_write = 0;
    uint64_t m_written = 0;
    uint64_t m_streamPos = 0;
};

using IqTailBuffer = TailBuffer<std::complex<float>>;

}
