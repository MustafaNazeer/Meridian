#include "meridian/book.hpp"

namespace meridian {

Book::Book(Symbol symbol) : symbol_(symbol) {}

Level* Book::best_bid() const noexcept {
    return bids_.empty() ? nullptr : bids_.begin()->second.get();
}

Level* Book::best_ask() const noexcept {
    return asks_.empty() ? nullptr : asks_.begin()->second.get();
}

void Book::add(Order* order) {
    Level* level = nullptr;
    if (order->side == Side::Buy) {
        auto it = bids_.find(order->price);
        if (it == bids_.end()) {
            auto inserted = bids_.emplace(order->price,
                                          std::make_unique<Level>(order->price));
            level = inserted.first->second.get();
        } else {
            level = it->second.get();
        }
    } else {
        auto it = asks_.find(order->price);
        if (it == asks_.end()) {
            auto inserted = asks_.emplace(order->price,
                                          std::make_unique<Level>(order->price));
            level = inserted.first->second.get();
        } else {
            level = it->second.get();
        }
    }
    level->append(order);
    id_index_[order->id] = order;
}

Order* Book::find(OrderId id) const noexcept {
    auto it = id_index_.find(id);
    return it == id_index_.end() ? nullptr : it->second;
}

Order* Book::remove_by_id(OrderId id) {
    auto it = id_index_.find(id);
    if (it == id_index_.end()) {
        return nullptr;
    }
    Order* order = it->second;
    Side side = order->side;
    Price price = order->price;

    if (side == Side::Buy) {
        auto level_it = bids_.find(price);
        if (level_it != bids_.end()) {
            level_it->second->remove(order);
            if (level_it->second->empty()) {
                bids_.erase(level_it);
            }
        }
    } else {
        auto level_it = asks_.find(price);
        if (level_it != asks_.end()) {
            level_it->second->remove(order);
            if (level_it->second->empty()) {
                asks_.erase(level_it);
            }
        }
    }

    id_index_.erase(it);
    return order;
}

void Book::erase_empty_level(Side side, Price price) {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        if (it != bids_.end() && it->second->empty()) {
            bids_.erase(it);
        }
    } else {
        auto it = asks_.find(price);
        if (it != asks_.end() && it->second->empty()) {
            asks_.erase(it);
        }
    }
}

void Book::publish_top_of_book(Timestamp ts) noexcept {
    TopOfBookSnapshot snap{};
    snap.ts = ts;
    if (!bids_.empty()) {
        const Level& level = *bids_.begin()->second;
        snap.best_bid_px  = level.price();
        snap.best_bid_qty = level.total_qty();
    }
    if (!asks_.empty()) {
        const Level& level = *asks_.begin()->second;
        snap.best_ask_px  = level.price();
        snap.best_ask_qty = level.total_qty();
    }
    snapshot_.write(snap);
}

void Book::publish_depth(Timestamp ts) noexcept {
    DepthSnapshot snap{};
    snap.ts = ts;
    std::size_t i = 0;
    for (auto it = bids_.begin(); it != bids_.end() && i < kDepthLevels; ++it, ++i) {
        const Level& level = *it->second;
        snap.bids[i].px          = level.price();
        snap.bids[i].qty         = level.total_qty();
        snap.bids[i].order_count = static_cast<std::uint32_t>(level.order_count());
    }
    snap.bid_levels = static_cast<std::uint8_t>(i);
    i = 0;
    for (auto it = asks_.begin(); it != asks_.end() && i < kDepthLevels; ++it, ++i) {
        const Level& level = *it->second;
        snap.asks[i].px          = level.price();
        snap.asks[i].qty         = level.total_qty();
        snap.asks[i].order_count = static_cast<std::uint32_t>(level.order_count());
    }
    snap.ask_levels = static_cast<std::uint8_t>(i);
    depth_.write(snap);
}

void Book::publish_trade(const TradePrint& p) noexcept {
    TradePrint out = p;
    out.seq = ++next_trade_seq_;
    trades_.push(out);
}

}  // namespace meridian
