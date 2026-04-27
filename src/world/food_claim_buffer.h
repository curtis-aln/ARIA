#pragma once

#include <cstdint>
#include <cstring>
#include <memory>

class FoodClaimBuffer
{
    std::unique_ptr<uint8_t[]> flags_;
    int                        capacity_ = 0;
    int                        active_ = 0;

public:
    FoodClaimBuffer() = default;

    // Non-copyable, non-movable — owns its buffer
    FoodClaimBuffer(const FoodClaimBuffer&) = delete;
    FoodClaimBuffer& operator=(const FoodClaimBuffer&) = delete;

    void reserve(const int n)
    {
        if (n <= capacity_)
            return;
        flags_ = std::make_unique<uint8_t[]>(n);
        capacity_ = n;
    }

    // Call once per frame before the parallel food phase
    void reset(const int active_count)
    {
        if (active_count > capacity_)
            reserve(active_count * 2);  // grow with headroom

        active_ = active_count;
        std::memset(flags_.get(), 0, static_cast<size_t>(active_));
    }

    // Returns true if this thread claimed the slot.
    // Uses a real atomic CAS so it is safe across threads.
    [[nodiscard]] bool claim(const int index)
    {
        // Cast to atomic for the CAS without storing atomic in the array.
        // This is valid on x86/x64 for naturally-aligned bytes.
        auto* a = reinterpret_cast<std::atomic<uint8_t>*>(&flags_[index]);
        uint8_t expected = 0;
        return a->compare_exchange_strong(
            expected, 1,
            std::memory_order_acquire,
            std::memory_order_relaxed);
    }

    [[nodiscard]] bool is_claimed(const int index) const
    {
        return flags_[index] != 0;
    }

    [[nodiscard]] int active_count() const { return active_; }
    [[nodiscard]] int capacity()     const { return capacity_; }
};