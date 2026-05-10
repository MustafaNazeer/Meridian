// ThreadSanitizer test for SeqlockSnapshot.
//
// One writer thread spins, repeatedly publishing a snapshot whose five
// fields encode the same monotonic counter via an injective relation:
//
//     bid_px  = c
//     bid_qty = c * 2
//     ask_px  = c + 100000
//     ask_qty = c * 3
//     ts      = c
//
// Two reader threads spin, calling SeqlockSnapshot::read(). Every
// successful read must satisfy the relation; if any read returns a
// snapshot whose fields straddle two writer publishes, the relation
// breaks and the test fails.
//
// The test runs for MERIDIAN_TSAN_DURATION_SEC seconds (default 3, env
// override). On TSAN-instrumented builds, ThreadSanitizer also fails
// the test if it observes a data race on any field. The C++ memory
// model treats every per-field std::atomic access as race-free, so the
// expectation is zero TSAN warnings under the agreed seqlock layout
// (see docs/adr/0003-seqlock-for-top-of-book.md).

#include "meridian/seqlock_snapshot.hpp"
#include "meridian/types.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <gtest/gtest.h>
#include <string>
#include <thread>

namespace meridian {
namespace {

constexpr int kDefaultDurationSec = 3;

int duration_seconds() {
    if (const char* env = std::getenv("MERIDIAN_TSAN_DURATION_SEC")) {
        try {
            const int v = std::stoi(env);
            if (v > 0) return v;
        } catch (...) {
            // fall through to default
        }
    }
    return kDefaultDurationSec;
}

TEST(SeqlockConcurrent, OneWriterTwoReadersAgreePerSnapshot) {
    SeqlockSnapshot snap;
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> writer_count{0};
    std::atomic<std::uint64_t> reader_count_a{0};
    std::atomic<std::uint64_t> reader_count_b{0};
    std::atomic<std::uint64_t> mismatches{0};

    auto writer = [&]() {
        std::uint64_t c = 1;  // start above the default zero so empty snapshots are detectable
        while (!stop.load(std::memory_order_relaxed)) {
            TopOfBookSnapshot in{};
            in.best_bid_px  = static_cast<Price>(c);
            in.best_bid_qty = static_cast<Quantity>(c * 2);
            in.best_ask_px  = static_cast<Price>(c + 100000);
            in.best_ask_qty = static_cast<Quantity>(c * 3);
            in.ts           = static_cast<Timestamp>(c);
            snap.write(in);
            writer_count.fetch_add(1, std::memory_order_relaxed);
            ++c;
        }
    };

    auto reader = [&](std::atomic<std::uint64_t>& my_count) {
        while (!stop.load(std::memory_order_relaxed)) {
            const TopOfBookSnapshot s = snap.read();
            // The initial all-zero snapshot is the one default state
            // before any writer publish. Skip it; the writer starts at
            // c = 1.
            if (s.best_bid_px == kInvalidPrice && s.best_ask_px == kInvalidPrice
                && s.best_bid_qty == 0 && s.best_ask_qty == 0 && s.ts == 0) {
                continue;
            }
            const auto c = static_cast<std::uint64_t>(s.best_bid_px);
            const bool ok =
                static_cast<std::uint64_t>(s.best_bid_qty) == c * 2 &&
                static_cast<std::uint64_t>(s.best_ask_px)  == c + 100000 &&
                static_cast<std::uint64_t>(s.best_ask_qty) == c * 3 &&
                static_cast<std::uint64_t>(s.ts)           == c;
            if (!ok) {
                mismatches.fetch_add(1, std::memory_order_relaxed);
            }
            my_count.fetch_add(1, std::memory_order_relaxed);
        }
    };

    const int seconds = duration_seconds();
    std::thread tw(writer);
    std::thread tra(reader, std::ref(reader_count_a));
    std::thread trb(reader, std::ref(reader_count_b));

    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    stop.store(true, std::memory_order_relaxed);
    tw.join();
    tra.join();
    trb.join();

    // Verify the threads actually did meaningful work; if the writer
    // or readers were starved (e.g. on a single-CPU CI runner) we want
    // a loud failure instead of a silent pass.
    EXPECT_GT(writer_count.load(), 0u);
    EXPECT_GT(reader_count_a.load(), 0u);
    EXPECT_GT(reader_count_b.load(), 0u);
    // The actual seqlock contract: every observed snapshot must
    // satisfy the writer's relation. Mismatches are a hard failure.
    EXPECT_EQ(mismatches.load(), 0u)
        << "writer published " << writer_count.load() << " snapshots; "
        << "reader A read " << reader_count_a.load() << " snapshots; "
        << "reader B read " << reader_count_b.load() << " snapshots; "
        << "torn-read mismatches: " << mismatches.load();
}

TEST(SeqlockConcurrent, ReaderSeesEveryWriterPublishGivenEnoughTime) {
    // A weaker but useful invariant: when the writer pauses, all
    // readers eventually converge on the latest published snapshot.
    SeqlockSnapshot snap;
    TopOfBookSnapshot in{};
    in.best_bid_px = 42;
    in.best_bid_qty = 84;
    in.best_ask_px = 142;
    in.best_ask_qty = 126;
    in.ts = 42;
    snap.write(in);

    std::atomic<bool> reader_done{false};
    TopOfBookSnapshot observed{};
    std::thread reader([&]() {
        observed = snap.read();
        reader_done.store(true, std::memory_order_release);
    });
    reader.join();

    EXPECT_TRUE(reader_done.load(std::memory_order_acquire));
    EXPECT_EQ(observed.best_bid_px, 42);
    EXPECT_EQ(observed.best_bid_qty, 84);
    EXPECT_EQ(observed.best_ask_px, 142);
    EXPECT_EQ(observed.best_ask_qty, 126);
    EXPECT_EQ(observed.ts, 42);
}

}  // namespace
}  // namespace meridian
