#pragma once

#include "meridian/depth_snapshot.hpp"
#include "meridian/level.hpp"
#include "meridian/seqlock_snapshot.hpp"
#include "meridian/trade_print.hpp"
#include "meridian/types.hpp"

#include <functional>
#include <map>
#include <memory>
#include <unordered_map>

namespace meridian {

class Book {
public:
    explicit Book(Symbol symbol);

    Book(const Book&) = delete;
    Book& operator=(const Book&) = delete;
    Book(Book&&) = delete;
    Book& operator=(Book&&) = delete;

    [[nodiscard]] Symbol symbol() const noexcept { return symbol_; }

    [[nodiscard]] Level* best_bid() const noexcept;
    [[nodiscard]] Level* best_ask() const noexcept;

    void add(Order* order);

    [[nodiscard]] Order* find(OrderId id) const noexcept;
    Order* remove_by_id(OrderId id);

    void erase_empty_level(Side side, Price price);

    // Recompute the top-of-book picture from the current bid/ask sides
    // and publish it through the seqlock-protected snapshot. Called by
    // MatchingEngine::apply after each event so any reader (the
    // sampler, the WebSocket server, a unit test) sees the post-event
    // top of book once the matching loop returns.
    void publish_top_of_book(Timestamp ts) noexcept;

    // Reader-side accessor: returns a consistent snapshot, retrying
    // internally if a publish is in flight. Safe to call from any
    // thread.
    [[nodiscard]] TopOfBookSnapshot top_of_book() const noexcept {
        return snapshot_.read();
    }

    // Publish the post-event L8 depth picture through the depth
    // seqlock. Walks the first kDepthLevels entries of each side's
    // price map. Called by MatchingEngine::apply after every accepted
    // event, immediately after publish_top_of_book.
    void publish_depth(Timestamp ts) noexcept;

    // Reader-side accessor for the L8 depth snapshot.
    [[nodiscard]] DepthSnapshot depth() const noexcept {
        return depth_.read();
    }

    // Publish a trade print into the per-Book ring. Called by
    // MatchingEngine::apply once per match leg. The caller leaves the
    // print's seq field at zero; this method assigns the monotonic
    // per-Book seq before writing.
    void publish_trade(const TradePrint& p) noexcept;

    // Reader-side accessor for the recent-prints ring. Copies up to
    // kTradeRing prints, oldest first, into `out` and writes the
    // populated count into `count`. Safe to call from any thread.
    void trades(std::array<TradePrint, kTradeRing>& out,
                std::size_t& count) const noexcept {
        trades_.read(out, count);
    }

    // Diagnostic accessor (tests and observability). Not for
    // application logic.
    [[nodiscard]] const SeqlockSnapshot& snapshot() const noexcept {
        return snapshot_;
    }

    // Diagnostic accessor (tests only). Walks the bid/ask price maps and
    // verifies that the cached top-of-book level pointers in the depth
    // cache match the first kDepthLevels entries of each side, in order.
    // Returns true when the cache is in sync; false on any mismatch.
    // Stub-implemented before the cache exists; will return true while
    // the cache is not yet maintained.
    [[nodiscard]] bool audit_depth_cache_for_test() const noexcept;

private:
    using BidLevelMap = std::map<Price, std::unique_ptr<Level>, std::greater<>>;
    using AskLevelMap = std::map<Price, std::unique_ptr<Level>>;

    friend class MatchingEngine;

    Symbol symbol_;
    BidLevelMap bids_;
    AskLevelMap asks_;
    std::unordered_map<OrderId, Order*> id_index_;
    SeqlockSnapshot snapshot_;
    SeqlockDepth depth_;
    TradeRing    trades_;
    std::uint64_t next_trade_seq_ = 0;  // single-writer monotonic counter
};

}  // namespace meridian
