// Single-threaded unit tests for LatencyHistogram.
//
// Concurrency behaviour (writer plus reader threads, TSAN clean) is
// covered by tests/concurrency/test_latency_histogram_concurrent.cpp.
// The tests here cover bucket boundaries, percentile computation, and
// the empty-histogram contract.

#include "meridian/latency_histogram.hpp"

#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>

namespace meridian {
namespace {

using ns = std::chrono::nanoseconds;

TEST(LatencyHistogram, EmptyHistogramReturnsZeroes) {
    LatencyHistogram h;
    const auto snap = h.snapshot();
    EXPECT_EQ(snap.samples, 0u);
    EXPECT_EQ(snap.p50_ns, 0u);
    EXPECT_EQ(snap.p99_ns, 0u);
    EXPECT_EQ(snap.p999_ns, 0u);
    EXPECT_EQ(snap.max_ns, 0u);
    for (auto c : snap.counts) EXPECT_EQ(c, 0u);
}

TEST(LatencyHistogram, BucketZeroCoversThirtyNs) {
    LatencyHistogram h;
    h.record(ns{30});
    h.record(ns{35});
    h.record(ns{41});  // just below 30 * sqrt(2) ~ 42.4
    const auto snap = h.snapshot();
    EXPECT_EQ(snap.samples, 3u);
    EXPECT_EQ(snap.counts[0], 3u);
    for (std::size_t i = 1; i < snap.counts.size(); ++i) {
        EXPECT_EQ(snap.counts[i], 0u) << "bucket " << i << " should be empty";
    }
}

TEST(LatencyHistogram, BucketBoundsAreLogSpaced) {
    LatencyHistogram h;
    // 30 ns * sqrt(2)^i bounds: 30, 42, 60, 84, 120, ...
    // Pick midpoints so each sample lands cleanly in its bucket.
    h.record(ns{30});      // bucket 0
    h.record(ns{45});      // bucket 1
    h.record(ns{65});      // bucket 2
    h.record(ns{90});      // bucket 3
    const auto snap = h.snapshot();
    EXPECT_EQ(snap.counts[0], 1u);
    EXPECT_EQ(snap.counts[1], 1u);
    EXPECT_EQ(snap.counts[2], 1u);
    EXPECT_EQ(snap.counts[3], 1u);
    EXPECT_EQ(snap.samples, 4u);
}

TEST(LatencyHistogram, OverflowBucketCatchesLargeSamples) {
    LatencyHistogram h;
    h.record(ns{5'000'000});   // 5 ms, well past the top regular bucket
    h.record(ns{50'000'000});  // 50 ms
    const auto snap = h.snapshot();
    EXPECT_EQ(snap.samples, 2u);
    EXPECT_EQ(snap.counts.back(), 2u);
}

TEST(LatencyHistogram, PercentilesFromKnownCdf) {
    LatencyHistogram h;
    // 1000 samples, all in bucket 0 (30 ns); p50, p99, p999 all
    // sit at bucket 0's lower bound (30 ns). max takes the lower
    // bound of the last non-empty bucket, which is also bucket 0
    // (30 ns) in this single-bucket case.
    for (int i = 0; i < 1000; ++i) h.record(ns{30});
    const auto snap = h.snapshot();
    EXPECT_EQ(snap.samples, 1000u);
    EXPECT_EQ(snap.p50_ns, 30u);
    EXPECT_EQ(snap.p99_ns, 30u);
    EXPECT_EQ(snap.p999_ns, 30u);
    EXPECT_EQ(snap.max_ns, 30u);
}

TEST(LatencyHistogram, PercentilesShiftWithDistribution) {
    LatencyHistogram h;
    // 990 samples in bucket 0, 10 samples in bucket 10 (a ~960 ns
    // range). p50 in bucket 0, p99 also in bucket 0 (count 990 covers
    // 99.0 percent), p999 in bucket 10 (we need 999/1000 to be covered
    // and only 990 are in bucket 0).
    for (int i = 0; i < 990; ++i) h.record(ns{30});
    for (int i = 0; i < 10; ++i)  h.record(ns{1000});
    const auto snap = h.snapshot();
    EXPECT_EQ(snap.samples, 1000u);
    EXPECT_EQ(snap.p50_ns, 30u);
    EXPECT_EQ(snap.p99_ns, 30u);
    EXPECT_GT(snap.p999_ns, 30u);
    EXPECT_GT(snap.max_ns, 30u);
}

}  // namespace
}  // namespace meridian
