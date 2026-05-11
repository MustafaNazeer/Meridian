#pragma once

// Seqlock-protected fixed-size ring of recent trade prints.
//
// One ring per Book (the matching thread is the single writer).
// Capacity kTradeRing = 16. push() advances a head index and bumps a
// seq counter using the same odd/even protocol as SeqlockSnapshot:
// readers may retry if they observe a write in progress, and the read
// is a snapshot of the entire ring under a retry loop.
//
// Insertion order is by the producer's seq field; readers receive a
// linearised view ordered oldest-first. When more than kTradeRing
// prints have been pushed, the oldest entries are silently dropped
// (the ring "forgets" prints beyond the most-recent kTradeRing).

#include "meridian/types.hpp"

#include <array>
#include <atomic>
#include <cstdint>

namespace meridian {

inline constexpr std::size_t kTradeRing = 16;

struct TradePrint {
    Timestamp ts = 0;
    Price price = kInvalidPrice;
    Quantity qty = 0;
    Side aggressor = Side::Buy;  // taker side
    std::uint64_t seq = 0;       // monotonic per Book
};

class TradeRing {
public:
    TradeRing() noexcept = default;

    TradeRing(const TradeRing&) = delete;
    TradeRing& operator=(const TradeRing&) = delete;

    // Writer: single thread (the matching thread). Writes the new
    // print into the next slot and bumps seq odd then even.
    // The even-seq release store at the end anchors slots_[slot] = p
    // before it (release prevents prior writes from being reordered
    // after it); on x86 with GCC/Clang the odd-seq release store also
    // acts as a compiler barrier preventing the data write from being
    // hoisted before it, but that is a compiler behaviour rather than
    // a C++20 memory model guarantee.
    void push(const TradePrint& p) noexcept {
        const std::uint64_t v = seq_.load(std::memory_order_relaxed);
        seq_.store(v + 1, std::memory_order_release);
        const std::size_t slot = next_slot_;
        slots_[slot] = p;
        next_slot_ = (slot + 1u) % kTradeRing;
        if (count_ < kTradeRing) ++count_;
        seq_.store(v + 2, std::memory_order_release);
    }

    // Reader: any thread. Copies up to kTradeRing prints into out,
    // oldest first, and writes the count of populated entries into
    // out_count. Retries internally on observed mid-read seq advance.
    void read(std::array<TradePrint, kTradeRing>& out,
              std::size_t& out_count) const noexcept {
        for (;;) {
            const std::uint64_t s1 = seq_.load(std::memory_order_acquire);
            if ((s1 & 1u) != 0u) continue;  // write in progress
            const std::size_t cnt   = count_;
            const std::size_t head  = next_slot_;  // one past the most recent slot
            std::array<TradePrint, kTradeRing> tmp{};
            // Reconstruct oldest-first: when cnt < kTradeRing, oldest
            // sits at slot 0 and there are cnt populated slots
            // contiguous; when cnt == kTradeRing, oldest sits at
            // slots_[head] and the ring is full.
            if (cnt < kTradeRing) {
                for (std::size_t i = 0; i < cnt; ++i) tmp[i] = slots_[i];
            } else {
                for (std::size_t i = 0; i < kTradeRing; ++i) {
                    tmp[i] = slots_[(head + i) % kTradeRing];
                }
            }
            const std::uint64_t s2 = seq_.load(std::memory_order_acquire);
            if (s1 == s2) {
                for (std::size_t i = 0; i < cnt; ++i) out[i] = tmp[i];
                out_count = cnt;
                return;
            }
        }
    }

    [[nodiscard]] std::uint64_t seq() const noexcept {
        return seq_.load(std::memory_order_acquire);
    }

private:
    alignas(64) std::atomic<std::uint64_t> seq_{0};
    std::array<TradePrint, kTradeRing> slots_{};
    // Plain ints rather than atomics: only the matching thread mutates
    // these, and readers re-read them under the seqlock retry loop.
    std::size_t next_slot_ = 0;
    std::size_t count_ = 0;
};

}  // namespace meridian
