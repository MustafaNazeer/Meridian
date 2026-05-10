#pragma once

// Seqlock-protected top-of-book snapshot.
//
// Single writer (the matching thread), many readers (the sampler thread,
// the WebSocket server, any future consumer). The publish path is lock
// free: the matching thread bumps a sequence counter to odd, writes the
// data fields, then bumps the counter to even. Readers load the
// counter, read the data, load the counter again, and retry if either
// load saw an odd counter or the two loads disagreed (a write was in
// progress).
//
// Memory model: per-field std::atomic with relaxed loads/stores for
// the data, std::memory_order_release on the writer's seq stores,
// std::memory_order_acquire on the reader's seq loads. The C++
// memory model treats every atomic operation as race-free regardless
// of ordering, so ThreadSanitizer stays silent on the data fields. The
// alternative (raw POD fields plus std::atomic_thread_fence) is faster
// per write but requires TSAN suppressions and is documented in
// docs/adr/0003-seqlock-for-top-of-book.md as the rejected option.
//
// The matching thread publishes once per accepted EngineEvent. Readers
// are expected to retry quickly; under sustained writer pressure a
// reader may livelock against the writer, which is acceptable for the
// 30 Hz sampler use case (the writer publishes far less often than the
// reader retries).

#include "meridian/types.hpp"

#include <atomic>
#include <cstdint>

namespace meridian {

// One consistent picture of the top of book at a single moment. Empty
// sides are signalled by kInvalidPrice on the price and a zero quantity.
// Readers should treat (px == kInvalidPrice) as "no side present" and
// must not derive a spread from a snapshot where either side is empty.
struct TopOfBookSnapshot {
    Price best_bid_px = kInvalidPrice;
    Quantity best_bid_qty = 0;
    Price best_ask_px = kInvalidPrice;
    Quantity best_ask_qty = 0;
    Timestamp ts = 0;  // event timestamp at which this snapshot was published

    [[nodiscard]] bool has_bid() const noexcept { return best_bid_px != kInvalidPrice; }
    [[nodiscard]] bool has_ask() const noexcept { return best_ask_px != kInvalidPrice; }
};

class SeqlockSnapshot {
public:
    SeqlockSnapshot() noexcept = default;

    SeqlockSnapshot(const SeqlockSnapshot&) = delete;
    SeqlockSnapshot& operator=(const SeqlockSnapshot&) = delete;

    // Writer: one and only the matching thread should call this.
    // Publishes a new snapshot atomically with respect to readers.
    void write(const TopOfBookSnapshot& s) noexcept {
        const std::uint64_t v = seq_.load(std::memory_order_relaxed);
        // Mark write in progress (odd seq). Release ensures any
        // happens-before edges from prior writes propagate.
        seq_.store(v + 1, std::memory_order_release);
        bid_px_.store(s.best_bid_px,   std::memory_order_relaxed);
        bid_qty_.store(s.best_bid_qty, std::memory_order_relaxed);
        ask_px_.store(s.best_ask_px,   std::memory_order_relaxed);
        ask_qty_.store(s.best_ask_qty, std::memory_order_relaxed);
        ts_.store(s.ts,                std::memory_order_relaxed);
        // Mark write complete (even seq). Release pairs with the
        // reader's acquire on its second seq load.
        seq_.store(v + 2, std::memory_order_release);
    }

    // Reader: any thread may call. Returns a consistent snapshot,
    // retrying internally if it observes a write in progress or a
    // concurrent write that completed mid-read.
    [[nodiscard]] TopOfBookSnapshot read() const noexcept {
        TopOfBookSnapshot out{};
        for (;;) {
            const std::uint64_t s1 = seq_.load(std::memory_order_acquire);
            if ((s1 & 1u) != 0u) continue;  // write in progress
            out.best_bid_px  = bid_px_.load(std::memory_order_relaxed);
            out.best_bid_qty = bid_qty_.load(std::memory_order_relaxed);
            out.best_ask_px  = ask_px_.load(std::memory_order_relaxed);
            out.best_ask_qty = ask_qty_.load(std::memory_order_relaxed);
            out.ts           = ts_.load(std::memory_order_relaxed);
            const std::uint64_t s2 = seq_.load(std::memory_order_acquire);
            if (s1 == s2) return out;
            // Counter advanced between the two loads; the data we read
            // may straddle a writer publish. Retry.
        }
    }

    // Bounded-retry read: returns std::nullopt if the snapshot did not
    // stabilise within max_retries attempts. Reserved for future
    // consumers that want to bound their tail latency under
    // pathological writer storms; not used in the matching loop or the
    // sampler today.
    [[nodiscard]] bool try_read(TopOfBookSnapshot& out,
                                std::size_t max_retries) const noexcept {
        for (std::size_t i = 0; i < max_retries; ++i) {
            const std::uint64_t s1 = seq_.load(std::memory_order_acquire);
            if ((s1 & 1u) != 0u) continue;
            out.best_bid_px  = bid_px_.load(std::memory_order_relaxed);
            out.best_bid_qty = bid_qty_.load(std::memory_order_relaxed);
            out.best_ask_px  = ask_px_.load(std::memory_order_relaxed);
            out.best_ask_qty = ask_qty_.load(std::memory_order_relaxed);
            out.ts           = ts_.load(std::memory_order_relaxed);
            const std::uint64_t s2 = seq_.load(std::memory_order_acquire);
            if (s1 == s2) return true;
        }
        return false;
    }

    // Diagnostic: the current sequence value. Intended for unit tests
    // and observability; not for application logic.
    [[nodiscard]] std::uint64_t seq() const noexcept {
        return seq_.load(std::memory_order_acquire);
    }

private:
    // Aligning seq_ to a cache line keeps reader spinning on its own
    // line away from any unrelated writer activity in the host object.
    alignas(64) std::atomic<std::uint64_t> seq_{0};
    std::atomic<Price>     bid_px_ {kInvalidPrice};
    std::atomic<Quantity>  bid_qty_{0};
    std::atomic<Price>     ask_px_ {kInvalidPrice};
    std::atomic<Quantity>  ask_qty_{0};
    std::atomic<Timestamp> ts_     {0};
};

}  // namespace meridian
