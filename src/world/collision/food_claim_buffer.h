#pragma once

#include <cstdint>
#include <cstring>
#include <memory>

class FoodClaimBuffer
{
    struct alignas(64) PaddedFlag {
        std::atomic<uint8_t> value{ 0 };
    };
    std::unique_ptr<PaddedFlag[]> flags_;
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
        flags_ = std::make_unique<PaddedFlag[]>(n);
        capacity_ = n;
    }

    // Call once per frame before the parallel food phase
    void reset(const int active_count)
    {
        if (active_count > capacity_)
            reserve(active_count * 2);

        active_ = active_count;
        std::memset(flags_.get(), 0, static_cast<size_t>(capacity_) * sizeof(PaddedFlag));
    }

    // Returns true if this thread claimed the slot.
    // Uses a real atomic CAS so it is safe across threads.
    [[nodiscard]] bool claim(const int index)
    {
        uint8_t expected = 0;
        return flags_[index].value.compare_exchange_strong(expected, 1,
            std::memory_order_acq_rel, std::memory_order_relaxed);
    }

    [[nodiscard]] bool is_claimed(const int index) const
    {
        return flags_[index].value != 0;
    }

    [[nodiscard]] int active_count() const { return active_; }
    [[nodiscard]] int capacity()     const { return capacity_; }
};