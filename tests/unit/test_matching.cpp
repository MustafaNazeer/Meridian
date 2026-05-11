#include "meridian/book.hpp"
#include "meridian/book_registry.hpp"
#include "meridian/matching.hpp"
#include "meridian/order_index.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace meridian {
namespace {

class MatchingTest : public ::testing::Test {
protected:
    OrderPool pool{1024};
    BookRegistry registry{1};
    OrderIndex index;
    Book& book = *registry.book(1);
    MatchingEngine engine{pool, registry, index};

    EngineEvent new_limit(OrderId id, Side side, Price px, Quantity qty,
                          Timestamp ts = 0) {
        return EngineEvent{
            .kind = EventKind::NewOrder,
            .symbol = 1,
            .ts = ts,
            .order_id = id,
            .side = side,
            .type = OrderType::Limit,
            .price = px,
            .qty = qty,
        };
    }

    EngineEvent new_market(OrderId id, Side side, Quantity qty,
                           Timestamp ts = 0) {
        return EngineEvent{
            .kind = EventKind::NewOrder,
            .symbol = 1,
            .ts = ts,
            .order_id = id,
            .side = side,
            .type = OrderType::Market,
            .price = 0,
            .qty = qty,
        };
    }

    EngineEvent new_ioc(OrderId id, Side side, Price px, Quantity qty,
                        Timestamp ts = 0) {
        return EngineEvent{
            .kind = EventKind::NewOrder,
            .symbol = 1,
            .ts = ts,
            .order_id = id,
            .side = side,
            .type = OrderType::IOC,
            .price = px,
            .qty = qty,
        };
    }

    EngineEvent new_cancel(OrderId id, Timestamp ts = 0) {
        return EngineEvent{
            .kind = EventKind::Cancel,
            .symbol = 1,
            .ts = ts,
            .order_id = id,
            .side = Side::Buy,  // unused for cancel
            .type = OrderType::Limit,
            .price = 0,
            .qty = 0,
        };
    }
};

// =============== Limit ===============

TEST_F(MatchingTest, LimitWithoutCounterpartyRestsAndAcknowledges) {
    auto reports = engine.apply(new_limit(1, Side::Buy, 100, 10));

    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[0].order_id, 1u);

    Level* level = book.best_bid();
    ASSERT_NE(level, nullptr);
    EXPECT_EQ(level->total_qty(), 10);
}

TEST_F(MatchingTest, LimitFullyCrossesSingleRestingOrderEmitsFill) {
    engine.apply(new_limit(1, Side::Sell, 100, 10));
    auto reports = engine.apply(new_limit(2, Side::Buy, 100, 10));

    // Per matching-semantics LIM-3: Ack first, then maker fill, then taker fill.
    ASSERT_EQ(reports.size(), 3u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[0].order_id, 2u);

    EXPECT_EQ(reports[1].kind, ReportKind::Fill);
    EXPECT_EQ(reports[1].order_id, 1u);              // maker
    EXPECT_EQ(reports[1].side, Side::Sell);
    EXPECT_EQ(reports[1].qty, 10);
    EXPECT_EQ(reports[1].price, 100);

    EXPECT_EQ(reports[2].kind, ReportKind::Fill);
    EXPECT_EQ(reports[2].order_id, 2u);              // taker
    EXPECT_EQ(reports[2].side, Side::Buy);
    EXPECT_EQ(reports[2].qty, 10);
    EXPECT_EQ(reports[2].price, 100);

    EXPECT_EQ(book.best_ask(), nullptr);
    EXPECT_EQ(book.best_bid(), nullptr);
}

TEST_F(MatchingTest, LimitPartiallyFillsAndRestsResidual) {
    engine.apply(new_limit(1, Side::Sell, 100, 10));
    auto reports = engine.apply(new_limit(2, Side::Buy, 100, 25));

    ASSERT_EQ(reports.size(), 3u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[1].kind, ReportKind::Fill);
    EXPECT_EQ(reports[1].order_id, 1u);
    EXPECT_EQ(reports[1].qty, 10);
    EXPECT_EQ(reports[2].kind, ReportKind::Fill);
    EXPECT_EQ(reports[2].order_id, 2u);
    EXPECT_EQ(reports[2].qty, 10);

    Level* bid = book.best_bid();
    ASSERT_NE(bid, nullptr);
    EXPECT_EQ(bid->total_qty(), 15);
    EXPECT_EQ(book.best_ask(), nullptr);
}

TEST_F(MatchingTest, LimitSweepsMultipleRestingOrdersFifoAtSamePrice) {
    engine.apply(new_limit(1, Side::Sell, 100, 10, /*ts=*/1));
    engine.apply(new_limit(2, Side::Sell, 100, 25, /*ts=*/2));
    auto reports = engine.apply(new_limit(3, Side::Buy, 100, 30));

    ASSERT_EQ(reports.size(), 5u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[1].order_id, 1u);
    EXPECT_EQ(reports[1].qty, 10);
    EXPECT_EQ(reports[2].order_id, 3u);
    EXPECT_EQ(reports[2].qty, 10);
    EXPECT_EQ(reports[3].order_id, 2u);
    EXPECT_EQ(reports[3].qty, 20);
    EXPECT_EQ(reports[4].order_id, 3u);
    EXPECT_EQ(reports[4].qty, 20);

    Level* ask = book.best_ask();
    ASSERT_NE(ask, nullptr);
    EXPECT_EQ(ask->total_qty(), 5);
}

TEST_F(MatchingTest, LimitSweepsMultiplePriceLevels) {
    engine.apply(new_limit(1, Side::Sell, 100, 5));
    engine.apply(new_limit(2, Side::Sell, 101, 10));
    auto reports = engine.apply(new_limit(3, Side::Buy, 102, 20));

    ASSERT_EQ(reports.size(), 5u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[1].price, 100);
    EXPECT_EQ(reports[1].qty, 5);
    EXPECT_EQ(reports[2].price, 100);
    EXPECT_EQ(reports[2].qty, 5);
    EXPECT_EQ(reports[3].price, 101);
    EXPECT_EQ(reports[3].qty, 10);
    EXPECT_EQ(reports[4].price, 101);
    EXPECT_EQ(reports[4].qty, 10);

    Level* bid = book.best_bid();
    ASSERT_NE(bid, nullptr);
    EXPECT_EQ(bid->price(), 102);
    EXPECT_EQ(bid->total_qty(), 5);
}

TEST_F(MatchingTest, LimitDoesNotCrossWhenPriceUnfavorable) {
    engine.apply(new_limit(1, Side::Sell, 105, 10));
    auto reports = engine.apply(new_limit(2, Side::Buy, 100, 10));

    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);

    Level* bid = book.best_bid();
    ASSERT_NE(bid, nullptr);
    EXPECT_EQ(bid->price(), 100);
    EXPECT_EQ(book.best_ask()->price(), 105);
}

// =============== Market ===============

TEST_F(MatchingTest, MarketOnEmptyBookRejectsWithEmptyBook) {
    auto reports = engine.apply(new_market(1, Side::Buy, 10));
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Reject);
    EXPECT_EQ(reports[0].reject_reason, RejectReason::EmptyBook);
}

TEST_F(MatchingTest, MarketFullyFillsAtBestPriceAcrossLevels) {
    engine.apply(new_limit(1, Side::Sell, 100, 5));
    engine.apply(new_limit(2, Side::Sell, 101, 5));
    auto reports = engine.apply(new_market(3, Side::Buy, 8));

    ASSERT_EQ(reports.size(), 5u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[0].qty, 8);
    EXPECT_EQ(reports[1].order_id, 1u);
    EXPECT_EQ(reports[1].price, 100);
    EXPECT_EQ(reports[1].qty, 5);
    EXPECT_EQ(reports[2].order_id, 3u);
    EXPECT_EQ(reports[2].price, 100);
    EXPECT_EQ(reports[2].qty, 5);
    EXPECT_EQ(reports[3].order_id, 2u);
    EXPECT_EQ(reports[3].price, 101);
    EXPECT_EQ(reports[3].qty, 3);
    EXPECT_EQ(reports[4].order_id, 3u);
    EXPECT_EQ(reports[4].price, 101);
    EXPECT_EQ(reports[4].qty, 3);
}

TEST_F(MatchingTest, MarketPartiallyFillsAndResidualIsImplicitlyCancelled) {
    engine.apply(new_limit(1, Side::Sell, 100, 5));
    auto reports = engine.apply(new_market(2, Side::Buy, 10));

    ASSERT_EQ(reports.size(), 3u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[0].qty, 10);
    EXPECT_EQ(reports[1].kind, ReportKind::Fill);
    EXPECT_EQ(reports[1].order_id, 1u);
    EXPECT_EQ(reports[1].qty, 5);
    EXPECT_EQ(reports[2].kind, ReportKind::Fill);
    EXPECT_EQ(reports[2].order_id, 2u);
    EXPECT_EQ(reports[2].qty, 5);

    EXPECT_EQ(book.best_bid(), nullptr);
    EXPECT_EQ(book.best_ask(), nullptr);
}

// =============== IOC ===============

TEST_F(MatchingTest, IocOnEmptyBookAcksWithSubmittedQty) {
    auto reports = engine.apply(new_ioc(1, Side::Buy, 100, 10));
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[0].qty, 10);
    EXPECT_EQ(book.best_bid(), nullptr);
}

TEST_F(MatchingTest, IocFillsWhatItCanAndDoesNotRest) {
    engine.apply(new_limit(1, Side::Sell, 100, 5));
    auto reports = engine.apply(new_ioc(2, Side::Buy, 100, 20));

    ASSERT_EQ(reports.size(), 3u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[0].qty, 20);
    EXPECT_EQ(reports[1].kind, ReportKind::Fill);
    EXPECT_EQ(reports[1].order_id, 1u);
    EXPECT_EQ(reports[1].qty, 5);
    EXPECT_EQ(reports[2].kind, ReportKind::Fill);
    EXPECT_EQ(reports[2].order_id, 2u);
    EXPECT_EQ(reports[2].qty, 5);

    EXPECT_EQ(book.best_bid(), nullptr);
    EXPECT_EQ(book.best_ask(), nullptr);
}

TEST_F(MatchingTest, IocPriceLimitedDoesNotCrossWorsePrices) {
    engine.apply(new_limit(1, Side::Sell, 100, 5));
    engine.apply(new_limit(2, Side::Sell, 105, 10));

    auto reports = engine.apply(new_ioc(3, Side::Buy, 102, 20));

    ASSERT_EQ(reports.size(), 3u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[0].qty, 20);
    EXPECT_EQ(reports[1].kind, ReportKind::Fill);
    EXPECT_EQ(reports[1].price, 100);
    EXPECT_EQ(reports[1].qty, 5);
    EXPECT_EQ(reports[2].kind, ReportKind::Fill);
    EXPECT_EQ(reports[2].price, 100);
    EXPECT_EQ(reports[2].qty, 5);

    Level* ask = book.best_ask();
    ASSERT_NE(ask, nullptr);
    EXPECT_EQ(ask->price(), 105);
    EXPECT_EQ(ask->total_qty(), 10);
}

// =============== Cancel ===============

TEST_F(MatchingTest, CancelKnownOrderEmitsCancelReport) {
    engine.apply(new_limit(1, Side::Buy, 100, 10));
    auto reports = engine.apply(new_cancel(1));

    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Cancel);
    EXPECT_EQ(reports[0].order_id, 1u);
    EXPECT_EQ(reports[0].qty, 10);
    EXPECT_EQ(reports[0].price, 100);

    EXPECT_EQ(book.best_bid(), nullptr);
}

TEST_F(MatchingTest, CancelUnknownOrderEmitsRejectNotFound) {
    auto reports = engine.apply(new_cancel(99));

    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Reject);
    EXPECT_EQ(reports[0].reject_reason, RejectReason::NotFound);
}

TEST_F(MatchingTest, CancelAfterFullFillEmitsRejectNotFound) {
    engine.apply(new_limit(1, Side::Sell, 100, 10));
    engine.apply(new_limit(2, Side::Buy, 100, 10));
    auto reports = engine.apply(new_cancel(1));

    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Reject);
    EXPECT_EQ(reports[0].reject_reason, RejectReason::NotFound);
}

TEST_F(MatchingTest, CancelAfterPartialFillRemovesResidual) {
    engine.apply(new_limit(1, Side::Sell, 100, 10));
    engine.apply(new_limit(2, Side::Buy, 100, 4));
    auto reports = engine.apply(new_cancel(1));

    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Cancel);
    EXPECT_EQ(reports[0].qty, 6);

    EXPECT_EQ(book.best_ask(), nullptr);
}

// =============== Post-only ===============

TEST_F(MatchingTest, PostOnlyOnEmptyBookRests) {
    EngineEvent ev{
        .kind = EventKind::NewOrder,
        .symbol = 1, .ts = 1, .order_id = 1,
        .side = Side::Buy, .type = OrderType::PostOnly,
        .price = 100, .qty = 10,
    };
    auto reports = engine.apply(ev);
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[0].reject_reason, RejectReason::None);
    ASSERT_NE(book.best_bid(), nullptr);
    EXPECT_EQ(book.best_bid()->price(), 100);
    EXPECT_EQ(book.best_bid()->total_qty(), 10);
}

TEST_F(MatchingTest, PostOnlyBuyAtBestAskRejectsWouldCross) {
    // Set up resting ask at 100.
    engine.apply(EngineEvent{EventKind::NewOrder, 1, 1, 1, Side::Sell,
                             OrderType::Limit, 100, 5});
    // Post-only buy at 100 would cross the resting ask: single Reject.
    auto reports = engine.apply(EngineEvent{EventKind::NewOrder, 1, 2, 2,
                                            Side::Buy, OrderType::PostOnly,
                                            100, 10});
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Reject);
    EXPECT_EQ(reports[0].reject_reason, RejectReason::WouldCross);
    EXPECT_EQ(reports[0].order_id, 2u);
    EXPECT_EQ(reports[0].qty, 10);
    // Resting ask at 100 unchanged.
    ASSERT_NE(book.best_ask(), nullptr);
    EXPECT_EQ(book.best_ask()->price(), 100);
    EXPECT_EQ(book.best_ask()->total_qty(), 5);
}

TEST_F(MatchingTest, PostOnlySellBelowBestBidRejectsWouldCross) {
    engine.apply(EngineEvent{EventKind::NewOrder, 1, 1, 1, Side::Buy,
                             OrderType::Limit, 100, 5});
    // Post-only sell at 100 would cross the resting bid: single Reject.
    auto reports = engine.apply(EngineEvent{EventKind::NewOrder, 1, 2, 2,
                                            Side::Sell, OrderType::PostOnly,
                                            100, 10});
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Reject);
    EXPECT_EQ(reports[0].reject_reason, RejectReason::WouldCross);
}

TEST_F(MatchingTest, PostOnlyBelowBestAskRests) {
    engine.apply(EngineEvent{EventKind::NewOrder, 1, 1, 1, Side::Sell,
                             OrderType::Limit, 105, 5});
    // Post-only buy at 100 (below best ask 105): no cross, ack and rest.
    auto reports = engine.apply(EngineEvent{EventKind::NewOrder, 1, 2, 2,
                                            Side::Buy, OrderType::PostOnly,
                                            100, 10});
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    ASSERT_NE(book.best_bid(), nullptr);
    EXPECT_EQ(book.best_bid()->price(), 100);
    ASSERT_NE(book.best_ask(), nullptr);
    EXPECT_EQ(book.best_ask()->price(), 105);
}

// =============== FOK ===============

TEST_F(MatchingTest, FokOnEmptyBookRejectsInsufficientLiquidity) {
    auto reports = engine.apply(EngineEvent{EventKind::NewOrder, 1, 1, 1,
                                            Side::Buy, OrderType::FOK,
                                            100, 10});
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Reject);
    EXPECT_EQ(reports[0].reject_reason, RejectReason::InsufficientLiquidity);
    EXPECT_EQ(reports[0].qty, 10);
}

TEST_F(MatchingTest, FokWithExactLiquidityFullyFills) {
    engine.apply(EngineEvent{EventKind::NewOrder, 1, 1, 1, Side::Sell,
                             OrderType::Limit, 100, 10});
    auto reports = engine.apply(EngineEvent{EventKind::NewOrder, 1, 2, 2,
                                            Side::Buy, OrderType::FOK,
                                            100, 10});
    ASSERT_EQ(reports.size(), 3u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[1].kind, ReportKind::Fill);  // maker
    EXPECT_EQ(reports[1].order_id, 1u);
    EXPECT_EQ(reports[2].kind, ReportKind::Fill);  // taker
    EXPECT_EQ(reports[2].order_id, 2u);
    EXPECT_EQ(book.best_ask(), nullptr);
    EXPECT_EQ(book.best_bid(), nullptr);
}

TEST_F(MatchingTest, FokWithInsufficientLiquidityRejects) {
    engine.apply(EngineEvent{EventKind::NewOrder, 1, 1, 1, Side::Sell,
                             OrderType::Limit, 100, 5});  // only 5 available
    auto reports = engine.apply(EngineEvent{EventKind::NewOrder, 1, 2, 2,
                                            Side::Buy, OrderType::FOK,
                                            100, 10});  // wants 10
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Reject);
    EXPECT_EQ(reports[0].reject_reason, RejectReason::InsufficientLiquidity);
    // Maker untouched: no partial fill on a rejected FOK.
    ASSERT_NE(book.best_ask(), nullptr);
    EXPECT_EQ(book.best_ask()->total_qty(), 5);
}

TEST_F(MatchingTest, FokSweepsTwoLevelsToFill) {
    engine.apply(EngineEvent{EventKind::NewOrder, 1, 1, 1, Side::Sell,
                             OrderType::Limit, 100, 4});
    engine.apply(EngineEvent{EventKind::NewOrder, 1, 2, 2, Side::Sell,
                             OrderType::Limit, 101, 6});
    auto reports = engine.apply(EngineEvent{EventKind::NewOrder, 1, 3, 3,
                                            Side::Buy, OrderType::FOK,
                                            101, 10});
    // Ack + 2 maker fills + 2 taker fills = 5 reports.
    ASSERT_EQ(reports.size(), 5u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    EXPECT_EQ(book.best_ask(), nullptr);
    EXPECT_EQ(book.best_bid(), nullptr);
}

TEST_F(MatchingTest, FokAtNonCrossingPriceRejects) {
    engine.apply(EngineEvent{EventKind::NewOrder, 1, 1, 1, Side::Sell,
                             OrderType::Limit, 105, 10});  // ask at 105
    auto reports = engine.apply(EngineEvent{EventKind::NewOrder, 1, 2, 2,
                                            Side::Buy, OrderType::FOK,
                                            100, 5});  // willing to pay only 100
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Reject);
    EXPECT_EQ(reports[0].reject_reason, RejectReason::InsufficientLiquidity);
}

// =============== Hot path discipline ===============

TEST_F(MatchingTest, SweepLoopMakesNoHeapAllocation) {
    // Pre-populate the book so Book::add is not on the sweep path.
    engine.apply(new_limit(1, Side::Sell, 100, 5));
    engine.apply(new_limit(2, Side::Sell, 101, 5));
    engine.apply(new_limit(3, Side::Sell, 102, 5));
    // The sweep below should match all three resting orders without
    // triggering the debug allocator override (which aborts the test
    // process if hit).
    auto reports = engine.apply(new_ioc(4, Side::Buy, 105, 15));
    EXPECT_EQ(reports.size(), 7u);  // 1 ack + 2 fills per match * 3 matches
}

TEST(MatchingEngine, ApplyRecordsExactlyOneLatencySample) {
    OrderPool pool(8);
    BookRegistry registry{1};
    OrderIndex index;
    MatchingEngine engine(pool, registry, index);
    EngineEvent ev{};
    ev.kind = EventKind::NewOrder; ev.symbol = 1; ev.ts = 1;
    ev.side = Side::Buy; ev.type = OrderType::Limit;
    ev.price = 100; ev.qty = 10; ev.order_id = 1;
    engine.apply(ev);
    const auto snap = engine.latency().snapshot();
    EXPECT_EQ(snap.samples, 1u);
}

TEST(MatchingEngine, MatchEmitsOneTradePrintPerMakerLeg) {
    OrderPool pool(8);
    BookRegistry registry{1};
    OrderIndex index;
    MatchingEngine engine(pool, registry, index);

    // Two resting bid levels (100 x 5 and 99 x 7). One incoming sell
    // for qty 8 matches first against 100 (5), then 99 (3). That is
    // two maker legs, so two trade prints.
    EngineEvent buy1{}; buy1.kind=EventKind::NewOrder; buy1.symbol=1;
    buy1.ts=1; buy1.side=Side::Buy; buy1.type=OrderType::Limit;
    buy1.price=100; buy1.qty=5; buy1.order_id=1; engine.apply(buy1);
    EngineEvent buy2 = buy1; buy2.ts=2; buy2.price=99; buy2.qty=7; buy2.order_id=2;
    engine.apply(buy2);

    EngineEvent sell{}; sell.kind=EventKind::NewOrder; sell.symbol=1;
    sell.ts=3; sell.side=Side::Sell; sell.type=OrderType::Limit;
    sell.price=99; sell.qty=8; sell.order_id=3;
    engine.apply(sell);

    Book* b = registry.book(1);
    std::array<TradePrint, kTradeRing> out{};
    std::size_t count = 0;
    b->trades(out, count);
    ASSERT_EQ(count, 2u);
    // First print: maker at 100, qty 5, aggressor sell.
    EXPECT_EQ(out[0].price, 100);
    EXPECT_EQ(out[0].qty, 5);
    EXPECT_EQ(out[0].aggressor, Side::Sell);
    // Second print: maker at 99, qty 3, aggressor sell.
    EXPECT_EQ(out[1].price, 99);
    EXPECT_EQ(out[1].qty, 3);
    EXPECT_EQ(out[1].aggressor, Side::Sell);
}

TEST(MatchingEngine, RejectedFokEmitsNoTradePrints) {
    OrderPool pool(8);
    BookRegistry registry{1};
    OrderIndex index;
    MatchingEngine engine(pool, registry, index);
    EngineEvent buy{}; buy.kind=EventKind::NewOrder; buy.symbol=1;
    buy.ts=1; buy.side=Side::Buy; buy.type=OrderType::Limit;
    buy.price=100; buy.qty=3; buy.order_id=1; engine.apply(buy);

    EngineEvent fok{}; fok.kind=EventKind::NewOrder; fok.symbol=1;
    fok.ts=2; fok.side=Side::Sell; fok.type=OrderType::FOK;
    fok.price=100; fok.qty=10; fok.order_id=2;  // unfillable
    engine.apply(fok);

    Book* b = registry.book(1);
    std::array<TradePrint, kTradeRing> out{};
    std::size_t count = 0;
    b->trades(out, count);
    EXPECT_EQ(count, 0u);
}

}  // namespace
}  // namespace meridian
