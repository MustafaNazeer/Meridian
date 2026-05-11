#pragma once

// Seqlock-protected L8 depth snapshot.
//
// Carries the top 8 price levels per side, each as (price, total
// quantity, order count). Lives alongside SeqlockSnapshot on Book and
// follows the same single-writer-many-reader idiom: writer (matching
// thread) bumps seq odd, writes the data, bumps seq even; reader
// retries if it observes a write in progress or a mid-read counter
// advance.
//
// Kept separate from SeqlockSnapshot so the TOB hot path stays
// unchanged for readers that only want top of book (the existing
// consumer of TopOfBookSnapshot). Depth readers (the sampler thread
// when emitting the v1.1 wire payload) pay the cost of the wider
// payload only when they need it.

#include "meridian/types.hpp"

#include <array>
#include <atomic>
#include <cstdint>

namespace meridian {

inline constexpr std::size_t kDepthLevels = 8;

struct DepthLevel {
    Price px = kInvalidPrice;
    Quantity qty = 0;
    std::uint32_t order_count = 0;
};

struct DepthSnapshot {
    std::array<DepthLevel, kDepthLevels> bids{};  // index 0 = best bid, descending
    std::array<DepthLevel, kDepthLevels> asks{};  // index 0 = best ask, ascending
    std::uint8_t bid_levels = 0;  // count of populated entries in bids
    std::uint8_t ask_levels = 0;
    Timestamp ts = 0;
};

class SeqlockDepth {
public:
    SeqlockDepth() noexcept = default;

    SeqlockDepth(const SeqlockDepth&) = delete;
    SeqlockDepth& operator=(const SeqlockDepth&) = delete;

    void write(const DepthSnapshot& s) noexcept {
        const std::uint64_t v = seq_.load(std::memory_order_relaxed);
        seq_.store(v + 1, std::memory_order_release);
        // The DepthSnapshot fields are POD; the seqlock retry contract
        // is the sole correctness guarantee (readers retry if their
        // two seq loads disagree). The data writes are non-atomic and
        // ordered after the odd-seq store and before the even-seq
        // store by the release semantics on those two stores.
        data_ = s;
        seq_.store(v + 2, std::memory_order_release);
    }

    [[nodiscard]] DepthSnapshot read() const noexcept {
        DepthSnapshot out{};
        for (;;) {
            const std::uint64_t s1 = seq_.load(std::memory_order_acquire);
            if ((s1 & 1u) != 0u) continue;  // write in progress
            out = data_;
            const std::uint64_t s2 = seq_.load(std::memory_order_acquire);
            if (s1 == s2) return out;
        }
    }

    [[nodiscard]] std::uint64_t seq() const noexcept {
        return seq_.load(std::memory_order_acquire);
    }

private:
    alignas(64) std::atomic<std::uint64_t> seq_{0};
    DepthSnapshot data_{};
};

}  // namespace meridian
