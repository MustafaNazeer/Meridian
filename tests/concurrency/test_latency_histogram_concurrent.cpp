// Concurrency test for LatencyHistogram.
//
// Many writers (simulating a potential future multi-thread engine)
// each record a known number of samples. Reader snapshots
// periodically; the final snapshot's total equals the sum of all
// writer counts. The histogram uses per-bucket std::atomic<uint64_t>
// counters with relaxed fetch_add and relaxed loads; this is
// TSAN-clean by construction, so this target is built under
// -fsanitize=thread along with test_seqlock_concurrent.

#include "meridian/latency_histogram.hpp"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace meridian {
namespace {

TEST(LatencyHistogramConcurrent, TotalSamplesEqualWritersSum) {
    LatencyHistogram h;
    constexpr int kWriters = 4;
    constexpr int kPerWriter = 25'000;
    std::vector<std::thread> writers;
    for (int t = 0; t < kWriters; ++t) {
        writers.emplace_back([&, t]() {
            // Stagger durations across the spectrum so multiple
            // buckets are exercised: 30 ns, 200 ns, 1000 ns rotating.
            const std::chrono::nanoseconds choices[3] = {
                std::chrono::nanoseconds{30},
                std::chrono::nanoseconds{200},
                std::chrono::nanoseconds{1000},
            };
            for (int i = 0; i < kPerWriter; ++i) {
                h.record(choices[(i + t) % 3]);
            }
        });
    }
    // Reader takes mid-flight snapshots; samples should never
    // decrease between consecutive snapshots (each snapshot total is
    // greater than or equal to the previous one).
    std::atomic<bool> stop{false};
    std::thread reader([&]() {
        std::uint64_t prev = 0;
        while (!stop.load(std::memory_order_relaxed)) {
            const auto snap = h.snapshot();
            EXPECT_GE(snap.samples, prev);
            prev = snap.samples;
        }
    });
    for (auto& t : writers) t.join();
    stop.store(true, std::memory_order_relaxed);
    reader.join();
    const auto final_snap = h.snapshot();
    EXPECT_EQ(final_snap.samples,
              static_cast<std::uint64_t>(kWriters * kPerWriter));
}

}  // namespace
}  // namespace meridian
