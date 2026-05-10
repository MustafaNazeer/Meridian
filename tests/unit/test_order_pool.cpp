#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <gtest/gtest.h>

namespace meridian {
namespace {

TEST(OrderPoolTest, AcquireReturnsNonNullSlotsUpToCapacity) {
    OrderPool pool(4);
    Order* a = pool.acquire();
    Order* b = pool.acquire();
    Order* c = pool.acquire();
    Order* d = pool.acquire();
    EXPECT_NE(a, nullptr);
    EXPECT_NE(b, nullptr);
    EXPECT_NE(c, nullptr);
    EXPECT_NE(d, nullptr);
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(c, d);
}

TEST(OrderPoolTest, AcquireBeyondCapacityReturnsNullptr) {
    OrderPool pool(2);
    EXPECT_NE(pool.acquire(), nullptr);
    EXPECT_NE(pool.acquire(), nullptr);
    EXPECT_EQ(pool.acquire(), nullptr);
}

TEST(OrderPoolTest, ReleaseReturnsSlotToPool) {
    OrderPool pool(1);
    Order* a = pool.acquire();
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(pool.acquire(), nullptr);
    pool.release(a);
    Order* b = pool.acquire();
    EXPECT_NE(b, nullptr);
    EXPECT_EQ(b, a);
}

TEST(OrderPoolTest, InUseCountTracksAcquiresAndReleases) {
    OrderPool pool(8);
    EXPECT_EQ(pool.in_use(), 0u);
    Order* a = pool.acquire();
    Order* b = pool.acquire();
    EXPECT_EQ(pool.in_use(), 2u);
    pool.release(a);
    EXPECT_EQ(pool.in_use(), 1u);
    pool.release(b);
    EXPECT_EQ(pool.in_use(), 0u);
}

}  // namespace
}  // namespace meridian
