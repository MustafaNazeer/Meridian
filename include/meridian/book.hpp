#pragma once

#include "meridian/level.hpp"
#include "meridian/types.hpp"

#include <functional>
#include <map>
#include <memory>
#include <unordered_map>

namespace meridian {

class Book {
public:
    explicit Book(Symbol symbol);

    [[nodiscard]] Symbol symbol() const noexcept { return symbol_; }

    [[nodiscard]] Level* best_bid() const noexcept;
    [[nodiscard]] Level* best_ask() const noexcept;

    void add(Order* order);

    [[nodiscard]] Order* find(OrderId id) const noexcept;
    Order* remove_by_id(OrderId id);

    void erase_empty_level(Side side, Price price);

private:
    using BidLevelMap = std::map<Price, std::unique_ptr<Level>, std::greater<>>;
    using AskLevelMap = std::map<Price, std::unique_ptr<Level>>;

    friend class MatchingEngine;

    Symbol symbol_;
    BidLevelMap bids_;
    AskLevelMap asks_;
    std::unordered_map<OrderId, Order*> id_index_;
};

}  // namespace meridian
