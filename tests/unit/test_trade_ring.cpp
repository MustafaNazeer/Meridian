// Single-threaded unit tests for TradeRing.
//
// Concurrency behaviour is covered by
// tests/concurrency/test_trade_ring_concurrent.cpp. The tests here
// cover the empty-ring contract, ordering by seq, wraparound past
// kTradeRing entries, and the partial-fill case where fewer than
// kTradeRing prints have been written.

#include "meridian/trade_print.hpp"
#include "meridian/types.hpp"

#include <array>
#include <gtest/gtest.h>

namespace meridian {
namespace {

TEST(TradeRing, EmptyRingReadsZeroCount) {
    TradeRing ring;
    std::array<TradePrint, kTradeRing> out{};
    std::size_t count = 0;
    ring.read(out, count);
    EXPECT_EQ(count, 0u);
}

TEST(TradeRing, OrderedByInsertionUpToCapacity) {
    TradeRing ring;
    for (std::uint64_t i = 1; i <= 5; ++i) {
        ring.push(TradePrint{
            .ts = static_cast<Timestamp>(i * 10),
            .price = static_cast<Price>(100 + i),
            .qty = static_cast<Quantity>(i),
            .aggressor = (i % 2 == 0) ? Side::Sell : Side::Buy,
            .seq = i,
        });
    }
    std::array<TradePrint, kTradeRing> out{};
    std::size_t count = 0;
    ring.read(out, count);
    ASSERT_EQ(count, 5u);
    for (std::size_t i = 0; i < count; ++i) {
        const auto exp_seq = static_cast<std::uint64_t>(i + 1);
        EXPECT_EQ(out[i].seq,       exp_seq)                              << "slot " << i;
        EXPECT_EQ(out[i].price,     static_cast<Price>(100 + i + 1))      << "slot " << i;
        EXPECT_EQ(out[i].ts,        static_cast<Timestamp>((i + 1) * 10)) << "slot " << i;
        EXPECT_EQ(out[i].qty,       static_cast<Quantity>(i + 1))         << "slot " << i;
        EXPECT_EQ(out[i].aggressor, (i % 2 == 0) ? Side::Buy : Side::Sell) << "slot " << i;
    }
}

TEST(TradeRing, WrapsPastCapacityAndKeepsLatest16) {
    TradeRing ring;
    for (std::uint64_t i = 1; i <= 20; ++i) {
        ring.push(TradePrint{
            .ts = static_cast<Timestamp>(i),
            .price = static_cast<Price>(100 + i),
            .qty = 1,
            .aggressor = Side::Buy,
            .seq = i,
        });
    }
    std::array<TradePrint, kTradeRing> out{};
    std::size_t count = 0;
    ring.read(out, count);
    EXPECT_EQ(count, kTradeRing);
    // After 20 pushes into a ring of size 16, the surviving prints are
    // seq 5 .. 20 (the 16 most recent), oldest first.
    for (std::size_t i = 0; i < count; ++i) {
        EXPECT_EQ(out[i].seq, static_cast<std::uint64_t>(i + 5));
    }
}

TEST(TradeRing, AggressorSideRoundTrips) {
    TradeRing ring;
    ring.push(TradePrint{.ts=1, .price=100, .qty=1, .aggressor=Side::Buy,  .seq=1});
    ring.push(TradePrint{.ts=2, .price=100, .qty=1, .aggressor=Side::Sell, .seq=2});
    std::array<TradePrint, kTradeRing> out{};
    std::size_t count = 0;
    ring.read(out, count);
    ASSERT_EQ(count, 2u);
    EXPECT_EQ(out[0].aggressor, Side::Buy);
    EXPECT_EQ(out[1].aggressor, Side::Sell);
}

TEST(TradeRing, EachPushAdvancesSeqByTwo) {
    TradeRing ring;
    EXPECT_EQ(ring.seq(), 0u);
    ring.push(TradePrint{.ts=1, .price=100, .qty=1, .aggressor=Side::Buy, .seq=1});
    EXPECT_EQ(ring.seq(), 2u);
    ring.push(TradePrint{.ts=2, .price=100, .qty=1, .aggressor=Side::Buy, .seq=2});
    EXPECT_EQ(ring.seq(), 4u);
}

TEST(TradeRing, AtCapacityMinusOneUsesStraightIndexing) {
    TradeRing ring;
    for (std::uint64_t i = 1; i < kTradeRing; ++i) {  // 15 pushes
        ring.push(TradePrint{
            .ts = static_cast<Timestamp>(i),
            .price = static_cast<Price>(200 + i),
            .qty = 1,
            .aggressor = Side::Buy,
            .seq = i,
        });
    }
    std::array<TradePrint, kTradeRing> out{};
    std::size_t count = 0;
    ring.read(out, count);
    ASSERT_EQ(count, kTradeRing - 1);
    for (std::size_t i = 0; i < count; ++i) {
        EXPECT_EQ(out[i].seq,   static_cast<std::uint64_t>(i + 1));
        EXPECT_EQ(out[i].price, static_cast<Price>(200 + i + 1));
    }
}

}  // namespace
}  // namespace meridian
