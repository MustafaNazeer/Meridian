// Single-threaded unit tests for SeqlockDepth.
//
// Concurrency behaviour is covered by
// tests/concurrency/test_seqlock_depth_concurrent.cpp. The tests here
// cover the empty-book contract, full L8 round trips, partial fills
// where one or both sides have fewer than 8 levels, and seq counter
// advancement.

#include "meridian/depth_snapshot.hpp"
#include "meridian/types.hpp"

#include <gtest/gtest.h>

namespace meridian {
namespace {

DepthSnapshot make(const std::initializer_list<DepthLevel>& bids,
                   const std::initializer_list<DepthLevel>& asks,
                   Timestamp ts) {
    DepthSnapshot s{};
    s.ts = ts;
    s.bid_levels = static_cast<std::uint8_t>(bids.size());
    s.ask_levels = static_cast<std::uint8_t>(asks.size());
    std::size_t i = 0;
    for (const auto& l : bids) { s.bids[i++] = l; }
    i = 0;
    for (const auto& l : asks) { s.asks[i++] = l; }
    return s;
}

TEST(SeqlockDepth, DefaultConstructionIsEmpty) {
    SeqlockDepth d;
    const auto v = d.read();
    EXPECT_EQ(v.bid_levels, 0u);
    EXPECT_EQ(v.ask_levels, 0u);
    EXPECT_EQ(v.ts, 0);
    EXPECT_EQ(v.bids[0].px, kInvalidPrice);
    EXPECT_EQ(v.asks[0].px, kInvalidPrice);
    EXPECT_EQ(d.seq(), 0u);
}

TEST(SeqlockDepth, FullL8RoundTrip) {
    SeqlockDepth d;
    const DepthSnapshot in = make(
        {{100, 50, 2}, {99, 40, 1}, {98, 30, 1}, {97, 20, 1},
         {96, 15, 1}, {95, 10, 1}, {94,  5, 1}, {93,  1, 1}},
        {{101, 60, 3}, {102, 45, 2}, {103, 35, 1}, {104, 25, 1},
         {105, 18, 1}, {106, 11, 1}, {107,  6, 1}, {108,  2, 1}},
        42);
    d.write(in);
    const auto out = d.read();
    EXPECT_EQ(out.bid_levels, 8u);
    EXPECT_EQ(out.ask_levels, 8u);
    EXPECT_EQ(out.ts, 42);
    for (std::size_t i = 0; i < kDepthLevels; ++i) {
        EXPECT_EQ(out.bids[i].px,          in.bids[i].px)          << "bid " << i;
        EXPECT_EQ(out.bids[i].qty,         in.bids[i].qty)         << "bid " << i;
        EXPECT_EQ(out.bids[i].order_count, in.bids[i].order_count) << "bid " << i;
        EXPECT_EQ(out.asks[i].px,          in.asks[i].px)          << "ask " << i;
        EXPECT_EQ(out.asks[i].qty,         in.asks[i].qty)         << "ask " << i;
        EXPECT_EQ(out.asks[i].order_count, in.asks[i].order_count) << "ask " << i;
    }
}

TEST(SeqlockDepth, PartialBidSideIsAllowed) {
    SeqlockDepth d;
    const DepthSnapshot in = make(
        {{100, 50, 1}, {99, 30, 1}, {98, 10, 1}},
        {{101, 40, 1}},
        7);
    d.write(in);
    const auto out = d.read();
    EXPECT_EQ(out.bid_levels, 3u);
    EXPECT_EQ(out.ask_levels, 1u);
    EXPECT_EQ(out.bids[0].px, 100);
    EXPECT_EQ(out.bids[2].px, 98);
    EXPECT_EQ(out.asks[0].px, 101);
}

TEST(SeqlockDepth, EmptySidesAreSignalledByLevelCount) {
    SeqlockDepth d;
    const DepthSnapshot in = make({}, {}, 1);
    d.write(in);
    const auto out = d.read();
    EXPECT_EQ(out.bid_levels, 0u);
    EXPECT_EQ(out.ask_levels, 0u);
}

TEST(SeqlockDepth, EachWriteAdvancesSeqByTwo) {
    SeqlockDepth d;
    EXPECT_EQ(d.seq(), 0u);
    const DepthSnapshot in = make({{100, 1, 1}}, {{101, 1, 1}}, 1);
    d.write(in);
    EXPECT_EQ(d.seq(), 2u);
    d.write(in);
    EXPECT_EQ(d.seq(), 4u);
}

}  // namespace
}  // namespace meridian
