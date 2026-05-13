#pragma once

// Lock-free engine latency histogram.
//
// 32 log-spaced buckets plus one overflow bucket, total 33. Bucket i
// (0 <= i < 32) covers the half-open range
//   [floor(30 ns * sqrt(2)^i), floor(30 ns * sqrt(2)^(i+1)))
// and bucket 32 catches everything from bucket 31's upper bound onward.
// The bucket bounds are computed at construction time into a constexpr
// table; the writer's record() does a small linear walk to find the
// bucket. With 32 buckets and the matching thread's per-event budget
// well under a microsecond, the walk is a few comparisons.
//
// Threading model: single writer (the matching thread), many readers
// (the sampler thread, observability, tests). Writer calls record()
// with one relaxed atomic increment. Readers call snapshot() which
// copies every bucket relaxed into a local array, sums them for the
// total samples, and computes p50 / p99 / p999 / max by walking the
// CDF. Percentile cell value is the *lower bound* of the bucket that
// contains the target rank (a conservative estimate of the percentile).

#include "meridian/types.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace meridian {

inline constexpr std::size_t kLatencyBuckets = 33;  // 32 log-spaced + 1 overflow

// Compile-time bucket bounds table.
//
// Returns an array of 33 entries where entry i is the lower bound of
// bucket i in nanoseconds. Bucket 0 starts at 30 ns. Each subsequent
// bound is multiplied by sqrt(2), rounded toward zero. Bucket 32 (the
// overflow bucket) has lower bound equal to the upper bound of bucket
// 31; samples >= that bound land in bucket 32.
[[nodiscard]] constexpr std::array<std::uint64_t, kLatencyBuckets>
latency_bucket_lower_bounds() noexcept {
    // sqrt(2) approximated to 18 digits; product is computed as a
    // double sequence and floored to uint64. 30 ns * sqrt(2)^32 is
    // 30 * 2^16 = 1'966'080 ns, just under 2 ms.
    constexpr double sqrt2 = 1.4142135623730951;
    std::array<std::uint64_t, kLatencyBuckets> out{};
    double cur = 30.0;
    out[0] = 30u;
    for (std::size_t i = 1; i < kLatencyBuckets; ++i) {
        cur *= sqrt2;
        out[i] = static_cast<std::uint64_t>(cur);
    }
    return out;
}

class LatencyHistogram {
public:
    struct Snapshot {
        std::array<std::uint64_t, kLatencyBuckets> counts{};
        std::uint64_t samples = 0;
        std::uint64_t p50_ns = 0;
        std::uint64_t p99_ns = 0;
        std::uint64_t p999_ns = 0;
        std::uint64_t max_ns = 0;
    };

    LatencyHistogram() noexcept = default;

    LatencyHistogram(const LatencyHistogram&) = delete;
    LatencyHistogram& operator=(const LatencyHistogram&) = delete;

    // Writer: single thread (the matching thread). One relaxed atomic
    // increment on the matched bucket. Negative durations are clamped
    // to bucket 0 (defensive; std::chrono::nanoseconds is signed but
    // steady_clock differences are non-negative in practice).
    void record(std::chrono::nanoseconds dur) noexcept {
        const std::int64_t n = dur.count();
        const std::uint64_t v = (n < 0) ? 0u : static_cast<std::uint64_t>(n);
        const auto& bounds = bounds_;
        std::size_t i = 0;
        // Walk forward until we find the bucket whose lower bound is
        // <= v but the next bucket's lower bound > v. The overflow
        // bucket is bucket kLatencyBuckets - 1.
        for (i = 0; i + 1 < kLatencyBuckets; ++i) {
            if (v < bounds[i + 1]) break;
        }
        counts_[i].fetch_add(1u, std::memory_order_relaxed);
    }

    // Reader: any thread. Copies all bucket counts relaxed, sums to
    // samples, computes p50 / p99 / p999 / max by walking the CDF.
    [[nodiscard]] Snapshot snapshot() const noexcept {
        Snapshot out{};
        std::uint64_t total = 0;
        for (std::size_t i = 0; i < kLatencyBuckets; ++i) {
            const std::uint64_t c = counts_[i].load(std::memory_order_relaxed);
            out.counts[i] = c;
            total += c;
        }
        out.samples = total;
        if (total == 0u) return out;

        // Targets: ceil(total * pct).
        auto rank_for = [total](double pct) -> std::uint64_t {
            const double t = static_cast<double>(total) * pct;
            const std::uint64_t r = static_cast<std::uint64_t>(t);
            // ceil: bump if there is any fractional component.
            return (static_cast<double>(r) < t) ? (r + 1u) : r;
        };
        const std::uint64_t r50  = rank_for(0.50);
        const std::uint64_t r99  = rank_for(0.99);
        const std::uint64_t r999 = rank_for(0.999);

        std::uint64_t cum = 0;
        bool got50 = false, got99 = false, got999 = false;
        std::size_t last_nonempty = 0;
        for (std::size_t i = 0; i < kLatencyBuckets; ++i) {
            cum += out.counts[i];
            if (out.counts[i] != 0u) last_nonempty = i;
            if (!got50  && cum >= r50)  { out.p50_ns  = bounds_[i]; got50  = true; }
            if (!got99  && cum >= r99)  { out.p99_ns  = bounds_[i]; got99  = true; }
            if (!got999 && cum >= r999) { out.p999_ns = bounds_[i]; got999 = true; }
        }
        // max: lower bound of the last non-empty bucket (matches the
        // convention used for the percentile cells; consistent visual
        // story across all four numerics).
        out.max_ns = bounds_[last_nonempty];
        return out;
    }

private:
    static constexpr std::array<std::uint64_t, kLatencyBuckets> bounds_ =
        latency_bucket_lower_bounds();
    std::array<std::atomic<std::uint64_t>, kLatencyBuckets> counts_{};
};

}  // namespace meridian
