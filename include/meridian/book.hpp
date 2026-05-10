#pragma once

#include "meridian/level.hpp"
#include "meridian/seqlock_snapshot.hpp"
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

    // Diagnostic accessor (tests and observability). Not for
    // application logic.
    [[nodiscard]] const SeqlockSnapshot& snapshot() const noexcept {
        return snapshot_;
    }

private:
    using BidLevelMap = std::map<Price, std::unique_ptr<Level>, std::greater<>>;
    using AskLevelMap = std::map<Price, std::unique_ptr<Level>>;

    friend class MatchingEngine;

    Symbol symbol_;
    BidLevelMap bids_;
    AskLevelMap asks_;
    std::unordered_map<OrderId, Order*> id_index_;
    SeqlockSnapshot snapshot_;
};

}  // namespace meridian
