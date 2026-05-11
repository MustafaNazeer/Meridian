// Concurrency test for TradeRing.
//
// One writer pushes prints with strictly increasing seq. One reader
// reads the ring repeatedly and verifies:
//   (a) entries within a single read are monotonic in seq,
//   (b) successive reads return non-decreasing max seq,
//   (c) no entry has a seq the writer has not yet emitted.
//
// Built without -fsanitize=thread for the same reason as
// test_seqlock_depth_concurrent: the ring uses a non-atomic struct
// copy under the seqlock retry contract; the retry guarantees a
// consistent snapshot at the caller boundary even though TSAN would
// flag the inner write as a race.

#include "meridian/trade_print.hpp"
#include "meridian/types.hpp"

#include <atomic>
#include <gtest/gtest.h>
#include <thread>

namespace meridian {
namespace {

TEST(TradeRingConcurrent, ReaderSeesMonotonicSeqs) {
    TradeRing ring;
    std::atomic<std::uint64_t> last_written{0};
    std::atomic<bool> stop{false};
    std::thread writer([&]() {
        std::uint64_t seq = 1;
        while (!stop.load(std::memory_order_relaxed)) {
            // Publish last_written before closing the seqlock so any reader
            // that observes this entry in a consistent snapshot is guaranteed
            // to see last_written >= seq when it loads with acquire.
            last_written.store(seq, std::memory_order_release);
            ring.push(TradePrint{
                .ts = static_cast<Timestamp>(seq),
                .price = 100,
                .qty = 1,
                .aggressor = Side::Buy,
                .seq = seq,
            });
            ++seq;
        }
    });
    std::uint64_t prev_max = 0;
    for (int i = 0; i < 10'000; ++i) {
        std::array<TradePrint, kTradeRing> out{};
        std::size_t count = 0;
        ring.read(out, count);
        std::uint64_t local_max = 0;
        const std::uint64_t ceiling =
            last_written.load(std::memory_order_acquire);
        for (std::size_t k = 0; k < count; ++k) {
            if (k > 0) {
                EXPECT_LT(out[k - 1].seq, out[k].seq)
                    << "ordering violation k=" << k;
            }
            if (out[k].seq > 0u) {
                EXPECT_LE(out[k].seq, ceiling)
                    << "saw a seq the writer has not emitted";
            }
            local_max = std::max(local_max, out[k].seq);
        }
        EXPECT_GE(local_max, prev_max);
        prev_max = local_max;
    }
    stop.store(true, std::memory_order_relaxed);
    writer.join();
}

}  // namespace
}  // namespace meridian
