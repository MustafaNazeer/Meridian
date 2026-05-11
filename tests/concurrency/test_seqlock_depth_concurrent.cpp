// Concurrency test for SeqlockDepth.
//
// One writer thread mutates the depth picture in a loop with a small
// rotating set of fingerprints (kMaxK + 1 distinct depth shapes).
// Several reader threads each verify that every snapshot they read
// matches one of the fingerprints the writer publishes (no torn
// reads, no fingerprints invented out of thin air). Mirrors
// test_seqlock_concurrent.cpp's structure.
//
// Note on sanitizers: SeqlockDepth deliberately uses a non-atomic
// struct copy under the seqlock retry contract. The seqlock retry
// guarantees readers never observe a torn snapshot at the caller
// boundary, but the inner write is a data race per the C++ memory
// model. This test is built without -fsanitize=thread (see the
// CMakeLists block for this target) so the race-free correctness
// property is verified end-to-end without TSAN-instrumenting the
// non-atomic copy.

#include "meridian/depth_snapshot.hpp"
#include "meridian/types.hpp"

#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace meridian {
namespace {

DepthSnapshot fingerprint(std::uint8_t k) {
    DepthSnapshot s{};
    s.ts = static_cast<Timestamp>(k);
    s.bid_levels = 8;
    s.ask_levels = 8;
    for (std::size_t i = 0; i < kDepthLevels; ++i) {
        s.bids[i] = DepthLevel{
            .px = static_cast<Price>(100 + k - static_cast<int>(i)),
            .qty = static_cast<Quantity>(10 + k),
            .order_count = static_cast<std::uint32_t>(1 + (k % 4)),
        };
        s.asks[i] = DepthLevel{
            .px = static_cast<Price>(200 + k + static_cast<int>(i)),
            .qty = static_cast<Quantity>(7 + k),
            .order_count = static_cast<std::uint32_t>(1 + ((k + 1) % 4)),
        };
    }
    return s;
}

bool matches_any_fingerprint(const DepthSnapshot& s, std::uint8_t max_k) {
    for (std::uint8_t k = 0; k <= max_k; ++k) {
        const auto f = fingerprint(k);
        bool eq = (s.ts == f.ts &&
                   s.bid_levels == f.bid_levels &&
                   s.ask_levels == f.ask_levels);
        for (std::size_t i = 0; eq && i < kDepthLevels; ++i) {
            if (s.bids[i].px != f.bids[i].px ||
                s.bids[i].qty != f.bids[i].qty ||
                s.bids[i].order_count != f.bids[i].order_count ||
                s.asks[i].px != f.asks[i].px ||
                s.asks[i].qty != f.asks[i].qty ||
                s.asks[i].order_count != f.asks[i].order_count) {
                eq = false;
            }
        }
        if (eq) return true;
    }
    return false;
}

TEST(SeqlockDepthConcurrent, ReaderNeverObservesTornSnapshot) {
    SeqlockDepth d;
    constexpr std::uint8_t kMaxK = 31;
    d.write(fingerprint(0));
    std::atomic<bool> stop{false};
    std::thread writer([&]() {
        std::uint8_t k = 0;
        while (!stop.load(std::memory_order_relaxed)) {
            d.write(fingerprint(k));
            k = (k + 1u) & kMaxK;
        }
    });
    constexpr int kReaders = 4;
    std::vector<std::thread> readers;
    std::atomic<std::uint64_t> total_reads{0};
    std::atomic<std::uint64_t> bad_reads{0};
    for (int t = 0; t < kReaders; ++t) {
        readers.emplace_back([&]() {
            for (int i = 0; i < 50'000; ++i) {
                const auto s = d.read();
                total_reads.fetch_add(1u, std::memory_order_relaxed);
                if (!matches_any_fingerprint(s, kMaxK)) {
                    bad_reads.fetch_add(1u, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& t : readers) t.join();
    stop.store(true, std::memory_order_relaxed);
    writer.join();
    EXPECT_GT(total_reads.load(), 0u);
    EXPECT_EQ(bad_reads.load(), 0u);
}

}  // namespace
}  // namespace meridian
