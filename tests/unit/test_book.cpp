#include "meridian/book.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <gtest/gtest.h>

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

TEST(BookTest, EmptyBookHasNoTopOfBook) {
    Book book{1};
    EXPECT_EQ(book.best_bid(), nullptr);
    EXPECT_EQ(book.best_ask(), nullptr);
}

TEST(BookTest, AddRestingOrderUpdatesTopOfBook) {
    OrderPool pool(8);
    Book book{1};

    book.add(make(pool, 1, Side::Buy,  100, 10));
    book.add(make(pool, 2, Side::Sell, 105, 20));

    ASSERT_NE(book.best_bid(), nullptr);
    EXPECT_EQ(book.best_bid()->price(), 100);
    EXPECT_EQ(book.best_bid()->total_qty(), 10);

    ASSERT_NE(book.best_ask(), nullptr);
    EXPECT_EQ(book.best_ask()->price(), 105);
    EXPECT_EQ(book.best_ask()->total_qty(), 20);
}

TEST(BookTest, BidsOrderedDescendingAsksAscending) {
    OrderPool pool(8);
    Book book{1};

    book.add(make(pool, 1, Side::Buy, 100, 10));
    book.add(make(pool, 2, Side::Buy,  99, 10));
    book.add(make(pool, 3, Side::Buy, 101, 10));

    book.add(make(pool, 4, Side::Sell, 110, 10));
    book.add(make(pool, 5, Side::Sell, 109, 10));

    EXPECT_EQ(book.best_bid()->price(), 101);
    EXPECT_EQ(book.best_ask()->price(), 109);
}

TEST(BookTest, FifoAtSamePrice) {
    OrderPool pool(8);
    Book book{1};

    book.add(make(pool, 1, Side::Buy, 100, 10));
    book.add(make(pool, 2, Side::Buy, 100, 25));

    Level* level = book.best_bid();
    ASSERT_NE(level, nullptr);
    EXPECT_EQ(level->order_count(), 2u);
    EXPECT_EQ(level->head()->id, 1u);
    EXPECT_EQ(level->head()->next->id, 2u);
}

TEST(BookTest, FindByIdReturnsRestingOrder) {
    OrderPool pool(8);
    Book book{1};

    Order* a = make(pool, 42, Side::Buy, 100, 10);
    book.add(a);

    EXPECT_EQ(book.find(42), a);
    EXPECT_EQ(book.find(999), nullptr);
}

TEST(BookTest, RemoveByIdSplicesAndForgetsId) {
    OrderPool pool(8);
    Book book{1};

    Order* a = make(pool, 1, Side::Buy, 100, 10);
    Order* b = make(pool, 2, Side::Buy, 100, 25);
    book.add(a);
    book.add(b);

    Order* removed = book.remove_by_id(1);
    EXPECT_EQ(removed, a);
    EXPECT_EQ(book.find(1), nullptr);

    Level* level = book.best_bid();
    ASSERT_NE(level, nullptr);
    EXPECT_EQ(level->order_count(), 1u);
    EXPECT_EQ(level->head(), b);
}

TEST(BookTest, RemoveLastOrderAtPriceErasesLevel) {
    OrderPool pool(4);
    Book book{1};

    Order* a = make(pool, 1, Side::Buy, 100, 10);
    book.add(a);

    book.remove_by_id(1);

    EXPECT_EQ(book.best_bid(), nullptr);
}

TEST(BookTest, RemoveByIdReturnsNullForUnknownId) {
    Book book{1};
    EXPECT_EQ(book.remove_by_id(99), nullptr);
}

// erase_empty_level is a public helper that the matching path does not
// invoke directly today (Book::remove_by_id handles level erasure
// inline). It is exercised here so the helper does not silently rot.
// All four observable branches (buy-side, ask-side, level not empty, no
// such level) are covered.

TEST(BookTest, EraseEmptyLevelRemovesEmptyBidLevel) {
    OrderPool pool(4);
    Book book{1};

    // Mirror of the ask-side test: drop the order via Level::remove
    // directly so the empty level is still present in the bid side map
    // when erase_empty_level runs. (Book::remove_by_id would have
    // erased the level inline; the helper is for paths that bypass it.)
    Order* a = make(pool, 1, Side::Buy, 100, 10);
    book.add(a);
    Level* level = book.best_bid();
    ASSERT_NE(level, nullptr);
    level->remove(a);
    EXPECT_TRUE(level->empty());
    ASSERT_NE(book.best_bid(), nullptr);

    book.erase_empty_level(Side::Buy, 100);
    EXPECT_EQ(book.best_bid(), nullptr);
}

TEST(BookTest, EraseEmptyLevelRemovesEmptyAskLevel) {
    OrderPool pool(4);
    Book book{1};

    // Construct a level directly through Level::remove so the level is
    // present in the side map but empty. The helper should remove it.
    Order* a = make(pool, 1, Side::Sell, 200, 10);
    book.add(a);
    // Drop the order from the level without going through
    // Book::remove_by_id (so the level remains in the side map).
    Level* level = book.best_ask();
    ASSERT_NE(level, nullptr);
    level->remove(a);
    EXPECT_TRUE(level->empty());
    // Side map still holds the empty level.
    ASSERT_NE(book.best_ask(), nullptr);

    book.erase_empty_level(Side::Sell, 200);
    EXPECT_EQ(book.best_ask(), nullptr);
}

TEST(BookTest, EraseEmptyLevelLeavesNonEmptyBidLevel) {
    OrderPool pool(4);
    Book book{1};

    Order* a = make(pool, 1, Side::Buy, 100, 10);
    book.add(a);
    book.erase_empty_level(Side::Buy, 100);
    // Level still has resting volume; helper must not erase it.
    Level* level = book.best_bid();
    ASSERT_NE(level, nullptr);
    EXPECT_EQ(level->total_qty(), 10);
}

TEST(BookTest, EraseEmptyLevelOnAbsentPriceIsNoOp) {
    Book book{1};
    // Both sides empty; both branches hit the "level not found" path.
    book.erase_empty_level(Side::Buy, 100);
    book.erase_empty_level(Side::Sell, 100);
    EXPECT_EQ(book.best_bid(), nullptr);
    EXPECT_EQ(book.best_ask(), nullptr);
}

}  // namespace
}  // namespace meridian
