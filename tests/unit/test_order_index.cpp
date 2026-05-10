#include "meridian/order_index.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <gtest/gtest.h>

namespace meridian {
namespace {

TEST(OrderIndexTest, EmptyIndexHasZeroSizeAndFindsNothing) {
    OrderIndex index;
    EXPECT_EQ(index.size(), 0u);
    EXPECT_FALSE(index.contains(42));
    EXPECT_EQ(index.find(42), nullptr);
}

TEST(OrderIndexTest, InsertReturnsIdentityOnFind) {
    OrderPool pool(4);
    OrderIndex index;
    Order* a = pool.acquire();
    a->id = 7;

    index.insert(7, a);

    EXPECT_EQ(index.size(), 1u);
    EXPECT_TRUE(index.contains(7));
    EXPECT_EQ(index.find(7), a);
}

TEST(OrderIndexTest, EraseRemovesEntryAndDoubleEraseIsNoOp) {
    OrderPool pool(4);
    OrderIndex index;
    Order* a = pool.acquire();

    index.insert(1, a);
    index.erase(1);

    EXPECT_EQ(index.size(), 0u);
    EXPECT_FALSE(index.contains(1));

    index.erase(1);  // no-op; must not throw
    EXPECT_EQ(index.size(), 0u);
}

TEST(OrderIndexTest, MultipleInsertsKeepIdentities) {
    OrderPool pool(8);
    OrderIndex index;
    Order* a = pool.acquire();
    Order* b = pool.acquire();
    Order* c = pool.acquire();

    index.insert(1, a);
    index.insert(2, b);
    index.insert(3, c);

    EXPECT_EQ(index.size(), 3u);
    EXPECT_EQ(index.find(1), a);
    EXPECT_EQ(index.find(2), b);
    EXPECT_EQ(index.find(3), c);
}

TEST(OrderIndexTest, ReinsertReplacesPreviousMapping) {
    OrderPool pool(4);
    OrderIndex index;
    Order* a = pool.acquire();
    Order* b = pool.acquire();

    index.insert(1, a);
    index.insert(1, b);

    EXPECT_EQ(index.size(), 1u);
    EXPECT_EQ(index.find(1), b);
}

}  // namespace
}  // namespace meridian
