#include "meridian/book.hpp"
#include "meridian/depth_snapshot.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace meridian {
namespace {

Order* make(OrderPool& pool, OrderId id, Side side, Price px, Quantity qty) {
    Order* o = pool.acquire();
    o->id = id;
    o->symbol = 1;
    o->side = side;
    o->type = OrderType::Limit;
    o->price = px;
    o->qty_remaining = qty;
    o->ts_arrival = 0;
    return o;
}

void assert_publish_depth_matches(Book& book, Timestamp ts) {
    book.publish_depth(ts);
    const DepthSnapshot snap = book.depth();
    EXPECT_TRUE(book.audit_depth_cache_for_test())
        << "cache audit failed after publish_depth at ts=" << ts;
    EXPECT_EQ(snap.ts, ts);
    EXPECT_LE(snap.bid_levels, kDepthLevels);
    EXPECT_LE(snap.ask_levels, kDepthLevels);
}

TEST(BookDepthCache, EmptyBookYieldsZeroLevels) {
    Book book{1};
    book.publish_depth(0);
    const DepthSnapshot snap = book.depth();
    EXPECT_EQ(snap.bid_levels, 0);
    EXPECT_EQ(snap.ask_levels, 0);
    EXPECT_TRUE(book.audit_depth_cache_for_test());
}

TEST(BookDepthCache, AddsWithinTopKAreOrdered) {
    OrderPool pool(64);
    Book book{1};
    const std::array<Price, 5> prices{100, 102, 104, 106, 108};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        book.add(make(pool, static_cast<OrderId>(i + 1), Side::Buy,
                      prices[i], 10));
        assert_publish_depth_matches(book, static_cast<Timestamp>(i));
    }
    const DepthSnapshot snap = book.depth();
    ASSERT_EQ(snap.bid_levels, 5);
    EXPECT_EQ(snap.bids[0].px, 108);
    EXPECT_EQ(snap.bids[1].px, 106);
    EXPECT_EQ(snap.bids[2].px, 104);
    EXPECT_EQ(snap.bids[3].px, 102);
    EXPECT_EQ(snap.bids[4].px, 100);
}

TEST(BookDepthCache, AddBeyondTopKEvictsWorst) {
    OrderPool pool(64);
    Book book{1};
    for (std::size_t i = 0; i < 9; ++i) {
        book.add(make(pool, static_cast<OrderId>(i + 1), Side::Sell,
                      static_cast<Price>(200 + i), 10));
        assert_publish_depth_matches(book, static_cast<Timestamp>(i));
    }
    const DepthSnapshot snap = book.depth();
    ASSERT_EQ(snap.ask_levels, kDepthLevels);
    for (std::size_t i = 0; i < kDepthLevels; ++i) {
        EXPECT_EQ(snap.asks[i].px, static_cast<Price>(200 + i))
            << "ask slot " << i;
    }
}

TEST(BookDepthCache, RemoveTopBidShiftsCacheUp) {
    OrderPool pool(64);
    Book book{1};
    for (std::size_t i = 0; i < 8; ++i) {
        book.add(make(pool, static_cast<OrderId>(i + 1), Side::Buy,
                      static_cast<Price>(100 + i), 10));
    }
    Order* top = book.find(8);
    ASSERT_NE(top, nullptr);
    book.remove_by_id(8);
    assert_publish_depth_matches(book, 99);
    const DepthSnapshot snap = book.depth();
    ASSERT_EQ(snap.bid_levels, 7);
    EXPECT_EQ(snap.bids[0].px, 106);
    EXPECT_EQ(snap.bids[6].px, 100);
}

TEST(BookDepthCache, RemoveInteriorCachedLevelRefillsFromMap) {
    OrderPool pool(64);
    Book book{1};
    for (std::size_t i = 0; i < 9; ++i) {
        book.add(make(pool, static_cast<OrderId>(i + 1), Side::Sell,
                      static_cast<Price>(200 + i), 10));
    }
    book.remove_by_id(4);
    assert_publish_depth_matches(book, 11);
    const DepthSnapshot snap = book.depth();
    ASSERT_EQ(snap.ask_levels, kDepthLevels);
    const std::array<Price, kDepthLevels> expected{
        200, 201, 202, 204, 205, 206, 207, 208};
    for (std::size_t i = 0; i < kDepthLevels; ++i) {
        EXPECT_EQ(snap.asks[i].px, expected[i]) << "ask slot " << i;
    }
}

TEST(BookDepthCache, PartialFillDoesNotChangeCachedLevelIdentity) {
    OrderPool pool(64);
    Book book{1};
    book.add(make(pool, 1, Side::Buy, 100, 10));
    book.add(make(pool, 2, Side::Buy, 100, 20));
    book.publish_depth(0);
    const DepthSnapshot before = book.depth();
    ASSERT_EQ(before.bid_levels, 1);
    EXPECT_EQ(before.bids[0].px, 100);
    EXPECT_EQ(before.bids[0].qty, 30);
    EXPECT_EQ(before.bids[0].order_count, 2);

    book.remove_by_id(1);
    book.publish_depth(1);
    const DepthSnapshot after = book.depth();
    ASSERT_EQ(after.bid_levels, 1);
    EXPECT_EQ(after.bids[0].px, 100);
    EXPECT_EQ(after.bids[0].qty, 20);
    EXPECT_EQ(after.bids[0].order_count, 1);
    EXPECT_TRUE(book.audit_depth_cache_for_test());
}

TEST(BookDepthCache, RandomSequencesAgreeWithReferenceMapWalk) {
    OrderPool pool(8192);
    Book book{1};
    std::mt19937 rng(0xC0FFEEu);
    std::uniform_int_distribution<int> kind_dist(0, 99);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> price_dist(80, 120);
    std::uniform_int_distribution<int> qty_dist(1, 50);

    std::vector<OrderId> live;
    OrderId next_id = 1;
    for (std::size_t step = 0; step < 2000; ++step) {
        const int k = kind_dist(rng);
        if (k < 70 || live.empty()) {
            Side side = (side_dist(rng) == 0) ? Side::Buy : Side::Sell;
            Price px = static_cast<Price>(price_dist(rng));
            Quantity qty = static_cast<Quantity>(qty_dist(rng));
            OrderId id = next_id++;
            book.add(make(pool, id, side, px, qty));
            live.push_back(id);
        } else {
            std::uniform_int_distribution<std::size_t> pick(0, live.size() - 1);
            const std::size_t i = pick(rng);
            book.remove_by_id(live[i]);
            live[i] = live.back();
            live.pop_back();
        }
        book.publish_depth(static_cast<Timestamp>(step));
        ASSERT_TRUE(book.audit_depth_cache_for_test())
            << "audit failed at step " << step;
    }
}

}  // namespace
}  // namespace meridian
