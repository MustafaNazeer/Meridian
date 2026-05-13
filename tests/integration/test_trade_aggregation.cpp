// Trade-print aggregation tested against worked-example scenarios.
// Each scenario drives the engine through a specific input sequence
// and asserts that the resulting trade ring matches the expected
// prints (price, qty, aggressor side, monotonic seq).

#include "meridian/book.hpp"
#include "meridian/book_registry.hpp"
#include "meridian/matching.hpp"
#include "meridian/order_index.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/trade_print.hpp"
#include "meridian/types.hpp"

#include <array>
#include <gtest/gtest.h>
#include <vector>

namespace meridian {
namespace {

struct ExpectedPrint {
    Price price;
    Quantity qty;
    Side aggressor;
};

void ExpectRing(const Book& book,
                const std::vector<ExpectedPrint>& expected) {
    std::array<TradePrint, kTradeRing> out{};
    std::size_t count = 0;
    book.trades(out, count);
    ASSERT_EQ(count, expected.size());
    for (std::size_t i = 0; i < count; ++i) {
        EXPECT_EQ(out[i].price, expected[i].price) << "i=" << i;
        EXPECT_EQ(out[i].qty, expected[i].qty) << "i=" << i;
        EXPECT_EQ(out[i].aggressor, expected[i].aggressor) << "i=" << i;
        if (i > 0) { EXPECT_LT(out[i - 1].seq, out[i].seq); }
    }
}

TEST(TradeAggregation, LimitSweepAcrossThreeMakerLegs) {
    OrderPool pool(8);
    BookRegistry registry{1};
    OrderIndex index;
    MatchingEngine engine(pool, registry, index);

    // Three resting asks at 100 (qty 4), 101 (qty 3), 102 (qty 2).
    // Total resting depth on the ask side: 9.
    auto place_ask = [&](OrderId id, Price p, Quantity q, Timestamp ts) {
        EngineEvent e{};
        e.kind = EventKind::NewOrder; e.symbol = 1; e.ts = ts;
        e.side = Side::Sell; e.type = OrderType::Limit;
        e.price = p; e.qty = q; e.order_id = id;
        engine.apply(e);
    };
    place_ask(1, 100, 4, 1);
    place_ask(2, 101, 3, 2);
    place_ask(3, 102, 2, 3);

    // Incoming buy at 102 for qty 8: matches 100x4, 101x3, 102x1. Three legs.
    EngineEvent buy{};
    buy.kind = EventKind::NewOrder; buy.symbol = 1; buy.ts = 4;
    buy.side = Side::Buy; buy.type = OrderType::Limit;
    buy.price = 102; buy.qty = 8; buy.order_id = 10;
    engine.apply(buy);

    Book* b = registry.book(1);
    ASSERT_NE(b, nullptr);
    ExpectRing(*b, {
        {100, 4, Side::Buy},
        {101, 3, Side::Buy},
        {102, 1, Side::Buy},
    });
}

TEST(TradeAggregation, IocPartialFillProducesOnePrintPerLeg) {
    OrderPool pool(8);
    BookRegistry registry{1};
    OrderIndex index;
    MatchingEngine engine(pool, registry, index);

    // One resting bid at 99 x 3.
    EngineEvent bid{};
    bid.kind = EventKind::NewOrder; bid.symbol = 1; bid.ts = 1;
    bid.side = Side::Buy; bid.type = OrderType::Limit;
    bid.price = 99; bid.qty = 3; bid.order_id = 1;
    engine.apply(bid);

    // IOC sell at 99 for qty 10: matches 99x3, the remaining 7 cancels.
    EngineEvent ioc{};
    ioc.kind = EventKind::NewOrder; ioc.symbol = 1; ioc.ts = 2;
    ioc.side = Side::Sell; ioc.type = OrderType::IOC;
    ioc.price = 99; ioc.qty = 10; ioc.order_id = 2;
    engine.apply(ioc);

    Book* b = registry.book(1);
    ExpectRing(*b, {{99, 3, Side::Sell}});
}

TEST(TradeAggregation, MarketConsumesEntireOppositeSide) {
    OrderPool pool(8);
    BookRegistry registry{1};
    OrderIndex index;
    MatchingEngine engine(pool, registry, index);

    auto place_bid = [&](OrderId id, Price p, Quantity q, Timestamp ts) {
        EngineEvent e{};
        e.kind = EventKind::NewOrder; e.symbol = 1; e.ts = ts;
        e.side = Side::Buy; e.type = OrderType::Limit;
        e.price = p; e.qty = q; e.order_id = id;
        engine.apply(e);
    };
    place_bid(1, 100, 5, 1);
    place_bid(2, 99,  5, 2);

    // Market sell for qty 10 consumes both levels.
    EngineEvent mkt{};
    mkt.kind = EventKind::NewOrder; mkt.symbol = 1; mkt.ts = 3;
    mkt.side = Side::Sell; mkt.type = OrderType::Market;
    mkt.price = 0; mkt.qty = 10; mkt.order_id = 9;
    engine.apply(mkt);

    Book* b = registry.book(1);
    ExpectRing(*b, {
        {100, 5, Side::Sell},
        {99,  5, Side::Sell},
    });
}

}  // namespace
}  // namespace meridian
