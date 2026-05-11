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
//
// Design note: where SeqlockSnapshot uses per-field std::atomic
// members and relaxed stores (per ADR 0003), SeqlockDepth uses a raw
// non-atomic struct copy data_ = s under the seqlock retry contract.
// The DepthSnapshot payload is approximately 200 bytes (16 DepthLevel
// entries plus headers), so per-field atomics would mean 48 plus
// atomic stores per publish on a hot path the matching thread takes
// after every accepted event. The raw struct copy is one mov-block.
// The seqlock retry on the reader is the correctness guarantee:
// readers retry on observed seq mismatch and never expose a torn
// state to the caller. ThreadSanitizer will flag data_ as a data
// race; the concurrency test at tests/concurrency/test_seqlock_depth_concurrent.cpp
// (lands in Task 8) carries the appropriate suppression. This
// trade-off is deliberate and is not a regression of ADR 0003 (which
// addresses the 24-byte top-of-book payload where per-field atomics
// are cheap).

#include "meridian/types.hpp"

#include <array>
#include <atomic>
#include <cstdint>

namespace meridian {

inline constexpr std::size_t kDepthLevels = 8;

static_assert(kDepthLevels <= 255,
              "DepthSnapshot::bid_levels and ask_levels are uint8_t; "
              "raising kDepthLevels above 255 requires a wider field");

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
        // data_ is a POD struct; its stores are non-atomic. The
        // seqlock retry contract (readers retry on seq mismatch) is
        // the sole correctness guarantee for readers. For the
        // writer's ordering, the even-seq release store at the end
        // anchors data_ = s before it (release prevents prior writes
        // from being reordered after it); on x86 with GCC/Clang the
        // odd-seq release store also acts as a compiler barrier
        // preventing data_ = s from being hoisted before it, but
        // that is a compiler behaviour rather than a C++20 memory
        // model guarantee.
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
