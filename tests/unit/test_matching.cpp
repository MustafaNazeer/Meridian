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

// =============== Phase 7 order types: stub reject ===============
// PostOnly and FOK are declared in OrderType for parity with Phase 7
// but are not implemented in Phase 1. The engine emits a Reject (with
// reason None as a sentinel; PostOnly's WouldCross and FOK's
// InsufficientLiquidity arrive in Phase 7) so a stray submission does
// not silently fall through. These tests exercise that defensive
// branch in matching.cpp's apply() switch so the line shows as covered
// in Phase 1 lcov reports rather than flagged as a Phase 7 gap. When
// Phase 7 lands, replace these tests with the real PostOnly and FOK
// behavior.

TEST_F(MatchingTest, PostOnlyOrderRejectsInPhase1) {
    EngineEvent ev{
        .kind = EventKind::NewOrder,
        .symbol = 1,
        .ts = 1,
        .order_id = 1,
        .side = Side::Buy,
        .type = OrderType::PostOnly,
        .price = 100,
        .qty = 10,
    };
    auto reports = engine.apply(ev);
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Reject);
    EXPECT_EQ(reports[0].order_id, 1u);
    EXPECT_EQ(reports[0].qty, 10);
    EXPECT_EQ(reports[0].price, 100);
    // Book unchanged: a Phase 7 stub reject must never rest.
    EXPECT_EQ(book.best_bid(), nullptr);
    EXPECT_EQ(book.best_ask(), nullptr);
}

TEST_F(MatchingTest, FokOrderRejectsInPhase1) {
    EngineEvent ev{
        .kind = EventKind::NewOrder,
        .symbol = 1,
        .ts = 1,
        .order_id = 1,
        .side = Side::Sell,
        .type = OrderType::FOK,
        .price = 200,
        .qty = 5,
    };
    auto reports = engine.apply(ev);
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Reject);
    EXPECT_EQ(reports[0].order_id, 1u);
    EXPECT_EQ(reports[0].qty, 5);
    EXPECT_EQ(reports[0].price, 200);
    EXPECT_EQ(book.best_bid(), nullptr);
    EXPECT_EQ(book.best_ask(), nullptr);
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

}  // namespace
}  // namespace meridian
