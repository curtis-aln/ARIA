#pragma once
#include <atomic>
#include <cstdint>

// Lock-free triple buffer for exactly ONE producer thread and ONE consumer thread.
//
// Three slots exist at all times, each owned by exactly one party:
//   back   - producer-private: the slot being written
//   front  - consumer-private: the slot being read
//   middle - shared mailbox (atomic), with a "dirty" bit meaning "holds an unread frame"
//
// publish()    : swap back <-> middle, marking the new middle dirty
// begin_read() : if middle is dirty, swap front <-> middle (clears dirty)
//
// Ownership only moves through a single atomic exchange, so the producer can
// never be handed a slot the consumer is (or is about to be) reading.
//
// NOTE: the slot returned by get_write_buffer() holds an OLD frame (from two
// publishes ago), not a blank one. The producer must fully overwrite/clear it.
template<typename T>
class TripleBuffer
{
public:
    // Arguments are forwarded (as const lvalues) to each of the three T's.
    // They can't be moved from, because they are used three times.
    template<typename... Args>
    explicit TripleBuffer(const Args&... args)
        : m_buffers{ T(args...), T(args...), T(args...) }   // C++17: elided, no move needed
    {}

    TripleBuffer(const TripleBuffer&) = delete;
    TripleBuffer& operator=(const TripleBuffer&) = delete;

    // ── Update (producer) thread ──────────────────────────────────────────

    T& get_write_buffer()
    {
        return m_buffers[m_back];
    }

    // Publish the frame just written; take ownership of the old middle slot.
    // acq_rel: release makes our writes visible to the consumer; acquire makes
    // the consumer's finished reads of the slot we receive happen-before our next writes.
    void publish()
    {
        const uint8_t old_middle =
            m_middle.exchange(static_cast<uint8_t>(m_back | kDirty), std::memory_order_acq_rel);
        m_back = static_cast<uint8_t>(old_middle & kIdxMask);
    }

    // ── Render (consumer) thread ──────────────────────────────────────────

    bool has_new_frame() const
    {
        return (m_middle.load(std::memory_order_relaxed) & kDirty) != 0;
    }

    // Returns the newest published frame (or the initial, constructed-but-empty
    // buffer if nothing has been published yet). The reference stays valid
    // until the next begin_read() call.
    const T& begin_read()
    {
        // Only the consumer clears the dirty bit, so if we see it set here it
        // stays set until our exchange below.
        if (m_middle.load(std::memory_order_relaxed) & kDirty)
        {
            const uint8_t old_middle = m_middle.exchange(m_front, std::memory_order_acq_rel);
            m_front = static_cast<uint8_t>(old_middle & kIdxMask);
        }
        return m_buffers[m_front];
    }

    void end_read() {} // kept for API symmetry / future metrics

private:
    static constexpr uint8_t kIdxMask = 0x3;
    static constexpr uint8_t kDirty = 0x4;

    T m_buffers[3];                                // real array: constructed/destroyed by the compiler

    alignas(64) std::atomic<uint8_t> m_middle{ 1 };// shared; own cache line
    alignas(64) uint8_t m_back = 0;               // producer thread only
    alignas(64) uint8_t m_front = 2;               // consumer thread only
};