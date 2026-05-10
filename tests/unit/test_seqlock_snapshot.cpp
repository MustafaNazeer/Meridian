// Single-threaded unit tests for SeqlockSnapshot.
//
// Concurrency behaviour (writer plus reader threads, TSAN clean) is
// covered by tests/concurrency/test_seqlock_concurrent.cpp. The tests
// here cover the data shape, the empty-side sentinels, and the seq
// counter advancement contract.

#include "meridian/seqlock_snapshot.hpp"
#include "meridian/types.hpp"

#include <gtest/gtest.h>

namespace meridian {
namespace {

TEST(SeqlockSnapshot, DefaultConstructionIsEmpty) {
    SeqlockSnapshot snap;
    const auto v = snap.read();
    EXPECT_EQ(v.best_bid_px, kInvalidPrice);
    EXPECT_EQ(v.best_bid_qty, 0);
    EXPECT_EQ(v.best_ask_px, kInvalidPrice);
    EXPECT_EQ(v.best_ask_qty, 0);
    EXPECT_EQ(v.ts, 0);
    EXPECT_FALSE(v.has_bid());
    EXPECT_FALSE(v.has_ask());
    EXPECT_EQ(snap.seq(), 0u);
}

TEST(SeqlockSnapshot, WriteRoundTrip) {
    SeqlockSnapshot snap;
    TopOfBookSnapshot in{};
    in.best_bid_px = 100;
    in.best_bid_qty = 25;
    in.best_ask_px = 102;
    in.best_ask_qty = 17;
    in.ts = 42;
    snap.write(in);
    const auto out = snap.read();
    EXPECT_EQ(out.best_bid_px, in.best_bid_px);
    EXPECT_EQ(out.best_bid_qty, in.best_bid_qty);
    EXPECT_EQ(out.best_ask_px, in.best_ask_px);
    EXPECT_EQ(out.best_ask_qty, in.best_ask_qty);
    EXPECT_EQ(out.ts, in.ts);
    EXPECT_TRUE(out.has_bid());
    EXPECT_TRUE(out.has_ask());
}

TEST(SeqlockSnapshot, EachWriteAdvancesSeqByTwo) {
    SeqlockSnapshot snap;
    EXPECT_EQ(snap.seq(), 0u);
    TopOfBookSnapshot in{};
    in.best_bid_px = 1;
    snap.write(in);
    EXPECT_EQ(snap.seq(), 2u);
    snap.write(in);
    EXPECT_EQ(snap.seq(), 4u);
    snap.write(in);
    EXPECT_EQ(snap.seq(), 6u);
}

TEST(SeqlockSnapshot, EmptyBidIsRepresentedBySentinel) {
    SeqlockSnapshot snap;
    TopOfBookSnapshot in{};
    in.best_bid_px = kInvalidPrice;
    in.best_bid_qty = 0;
    in.best_ask_px = 200;
    in.best_ask_qty = 5;
    in.ts = 7;
    snap.write(in);
    const auto out = snap.read();
    EXPECT_FALSE(out.has_bid());
    EXPECT_TRUE(out.has_ask());
    EXPECT_EQ(out.best_ask_px, 200);
}

TEST(SeqlockSnapshot, EmptyAskIsRepresentedBySentinel) {
    SeqlockSnapshot snap;
    TopOfBookSnapshot in{};
    in.best_bid_px = 99;
    in.best_bid_qty = 3;
    in.best_ask_px = kInvalidPrice;
    in.best_ask_qty = 0;
    in.ts = 8;
    snap.write(in);
    const auto out = snap.read();
    EXPECT_TRUE(out.has_bid());
    EXPECT_FALSE(out.has_ask());
    EXPECT_EQ(out.best_bid_px, 99);
}

TEST(SeqlockSnapshot, TryReadSucceedsOnStableSnapshot) {
    SeqlockSnapshot snap;
    TopOfBookSnapshot in{};
    in.best_bid_px = 50;
    in.best_ask_px = 51;
    in.ts = 1;
    snap.write(in);
    TopOfBookSnapshot out{};
    EXPECT_TRUE(snap.try_read(out, 1));  // one attempt is enough
    EXPECT_EQ(out.best_bid_px, 50);
    EXPECT_EQ(out.best_ask_px, 51);
}

TEST(SeqlockSnapshot, OverwriteReplacesPreviousSnapshot) {
    SeqlockSnapshot snap;
    TopOfBookSnapshot a{};
    a.best_bid_px = 10;
    a.best_ask_px = 11;
    a.ts = 1;
    snap.write(a);
    TopOfBookSnapshot b{};
    b.best_bid_px = 20;
    b.best_ask_px = 21;
    b.ts = 2;
    snap.write(b);
    const auto out = snap.read();
    EXPECT_EQ(out.best_bid_px, 20);
    EXPECT_EQ(out.best_ask_px, 21);
    EXPECT_EQ(out.ts, 2);
}

}  // namespace
}  // namespace meridian
