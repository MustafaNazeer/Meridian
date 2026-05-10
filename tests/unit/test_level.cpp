#include "meridian/level.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <gtest/gtest.h>

namespace meridian {
namespace {

Order* make_order(OrderPool& pool, OrderId id, Quantity qty, Price price = 100) {
    Order* o = pool.acquire();
    o->id = id;
    o->symbol = 1;
    o->side = Side::Buy;
    o->type = OrderType::Limit;
    o->price = price;
    o->qty_remaining = qty;
    o->ts_arrival = 0;
    return o;
}

TEST(LevelTest, EmptyLevelHasZeroTotalsAndNoHead) {
    Level level{100};
    EXPECT_EQ(level.price(), 100);
    EXPECT_EQ(level.total_qty(), 0);
    EXPECT_EQ(level.order_count(), 0u);
    EXPECT_EQ(level.head(), nullptr);
    EXPECT_TRUE(level.empty());
}

TEST(LevelTest, AppendInsertsAtTailAndUpdatesTotals) {
    OrderPool pool(4);
    Level level{100};

    Order* a = make_order(pool, 1, 10);
    Order* b = make_order(pool, 2, 25);
    level.append(a);
    level.append(b);

    EXPECT_EQ(level.head(), a);
    EXPECT_EQ(a->next, b);
    EXPECT_EQ(b->prev, a);
    EXPECT_EQ(b->next, nullptr);
    EXPECT_EQ(level.total_qty(), 35);
    EXPECT_EQ(level.order_count(), 2u);
}

TEST(LevelTest, RemoveHeadAdvancesAndDecrementsTotals) {
    OrderPool pool(4);
    Level level{100};

    Order* a = make_order(pool, 1, 10);
    Order* b = make_order(pool, 2, 25);
    level.append(a);
    level.append(b);

    level.remove(a);

    EXPECT_EQ(level.head(), b);
    EXPECT_EQ(b->prev, nullptr);
    EXPECT_EQ(level.total_qty(), 25);
    EXPECT_EQ(level.order_count(), 1u);
}

TEST(LevelTest, RemoveMiddleSplicesAndDecrementsTotals) {
    OrderPool pool(4);
    Level level{100};

    Order* a = make_order(pool, 1, 10);
    Order* b = make_order(pool, 2, 25);
    Order* c = make_order(pool, 3, 5);
    level.append(a);
    level.append(b);
    level.append(c);

    level.remove(b);

    EXPECT_EQ(level.head(), a);
    EXPECT_EQ(a->next, c);
    EXPECT_EQ(c->prev, a);
    EXPECT_EQ(level.total_qty(), 15);
    EXPECT_EQ(level.order_count(), 2u);
}

TEST(LevelTest, RemoveOnlyOrderEmptiesLevel) {
    OrderPool pool(4);
    Level level{100};

    Order* a = make_order(pool, 1, 10);
    level.append(a);
    level.remove(a);

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.head(), nullptr);
    EXPECT_EQ(level.tail(), nullptr);
    EXPECT_EQ(level.total_qty(), 0);
    EXPECT_EQ(level.order_count(), 0u);
}

TEST(LevelTest, AdjustQuantityUpdatesTotalQty) {
    OrderPool pool(4);
    Level level{100};

    Order* a = make_order(pool, 1, 10);
    level.append(a);

    level.adjust_qty(a, /*new_remaining=*/3);

    EXPECT_EQ(a->qty_remaining, 3);
    EXPECT_EQ(level.total_qty(), 3);
}

}  // namespace
}  // namespace meridian
