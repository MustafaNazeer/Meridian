#include "meridian/book.hpp"

#include <algorithm>

namespace meridian {

Book::Book(Symbol symbol, bool observability)
    : symbol_(symbol), observability_(observability) {}

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
            insert_cache_bid(level);
        } else {
            level = it->second.get();
        }
    } else {
        auto it = asks_.find(order->price);
        if (it == asks_.end()) {
            auto inserted = asks_.emplace(order->price,
                                          std::make_unique<Level>(order->price));
            level = inserted.first->second.get();
            insert_cache_ask(level);
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
            Level* level_ptr = level_it->second.get();
            level_it->second->remove(order);
            if (level_it->second->empty()) {
                erase_cache_bid(level_ptr);
                bids_.erase(level_it);
            }
        }
    } else {
        auto level_it = asks_.find(price);
        if (level_it != asks_.end()) {
            Level* level_ptr = level_it->second.get();
            level_it->second->remove(order);
            if (level_it->second->empty()) {
                erase_cache_ask(level_ptr);
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
            Level* level_ptr = it->second.get();
            erase_cache_bid(level_ptr);
            bids_.erase(it);
        }
    } else {
        auto it = asks_.find(price);
        if (it != asks_.end() && it->second->empty()) {
            Level* level_ptr = it->second.get();
            erase_cache_ask(level_ptr);
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
    for (std::size_t i = 0; i < bid_cache_count_; ++i) {
        const Level& level = *cached_bids_[i];
        snap.bids[i].px          = level.price();
        snap.bids[i].qty         = level.total_qty();
        snap.bids[i].order_count = static_cast<std::uint32_t>(level.order_count());
    }
    snap.bid_levels = bid_cache_count_;
    for (std::size_t i = 0; i < ask_cache_count_; ++i) {
        const Level& level = *cached_asks_[i];
        snap.asks[i].px          = level.price();
        snap.asks[i].qty         = level.total_qty();
        snap.asks[i].order_count = static_cast<std::uint32_t>(level.order_count());
    }
    snap.ask_levels = ask_cache_count_;
    depth_.write(snap);
}

void Book::publish_trade(const TradePrint& p) noexcept {
    TradePrint out = p;
    out.seq = ++next_trade_seq_;
    trades_.push(out);
}

void Book::insert_cache_bid(Level* level) noexcept {
    if (!observability_) return;
    const Price p = level->price();
    std::size_t r = 0;
    while (r < bid_cache_count_ && cached_bids_[r]->price() > p) ++r;
    if (r >= kDepthLevels) return;
    const std::size_t shift_end =
        (bid_cache_count_ < kDepthLevels) ? bid_cache_count_
                                          : (kDepthLevels - 1);
    for (std::size_t i = shift_end; i > r; --i) {
        cached_bids_[i] = cached_bids_[i - 1];
    }
    cached_bids_[r] = level;
    if (bid_cache_count_ < kDepthLevels) ++bid_cache_count_;
}

void Book::insert_cache_ask(Level* level) noexcept {
    if (!observability_) return;
    const Price p = level->price();
    std::size_t r = 0;
    while (r < ask_cache_count_ && cached_asks_[r]->price() < p) ++r;
    if (r >= kDepthLevels) return;
    const std::size_t shift_end =
        (ask_cache_count_ < kDepthLevels) ? ask_cache_count_
                                          : (kDepthLevels - 1);
    for (std::size_t i = shift_end; i > r; --i) {
        cached_asks_[i] = cached_asks_[i - 1];
    }
    cached_asks_[r] = level;
    if (ask_cache_count_ < kDepthLevels) ++ask_cache_count_;
}

void Book::erase_cache_bid(Level* level) noexcept {
    if (!observability_) return;
    std::size_t p = 0;
    while (p < bid_cache_count_ && cached_bids_[p] != level) ++p;
    if (p == bid_cache_count_) return;
    for (std::size_t i = p + 1; i < bid_cache_count_; ++i) {
        cached_bids_[i - 1] = cached_bids_[i];
    }
    --bid_cache_count_;
    cached_bids_[bid_cache_count_] = nullptr;
    // Refill the freed slot if the post-erase map will still have more
    // levels than the cache. bids_.size() still counts the level being
    // erased (call site invokes us before bids_.erase), so compare
    // against bid_cache_count_ + 1 to discount it. The find+advance can
    // also land on the level being erased when we just shifted out the
    // last cached entry; skip past it.
    if (bid_cache_count_ < kDepthLevels &&
        bids_.size() > static_cast<std::size_t>(bid_cache_count_) + 1) {
        auto it = bids_.end();
        if (bid_cache_count_ == 0) {
            it = bids_.begin();
        } else {
            it = bids_.find(cached_bids_[bid_cache_count_ - 1]->price());
            if (it != bids_.end()) ++it;
        }
        if (it != bids_.end() && it->second.get() == level) ++it;
        if (it != bids_.end()) {
            cached_bids_[bid_cache_count_] = it->second.get();
            ++bid_cache_count_;
        }
    }
}

void Book::erase_cache_ask(Level* level) noexcept {
    if (!observability_) return;
    std::size_t p = 0;
    while (p < ask_cache_count_ && cached_asks_[p] != level) ++p;
    if (p == ask_cache_count_) return;
    for (std::size_t i = p + 1; i < ask_cache_count_; ++i) {
        cached_asks_[i - 1] = cached_asks_[i];
    }
    --ask_cache_count_;
    cached_asks_[ask_cache_count_] = nullptr;
    if (ask_cache_count_ < kDepthLevels &&
        asks_.size() > static_cast<std::size_t>(ask_cache_count_) + 1) {
        auto it = asks_.end();
        if (ask_cache_count_ == 0) {
            it = asks_.begin();
        } else {
            it = asks_.find(cached_asks_[ask_cache_count_ - 1]->price());
            if (it != asks_.end()) ++it;
        }
        if (it != asks_.end() && it->second.get() == level) ++it;
        if (it != asks_.end()) {
            cached_asks_[ask_cache_count_] = it->second.get();
            ++ask_cache_count_;
        }
    }
}

bool Book::audit_depth_cache_for_test() const noexcept {
    if (!observability_) return true;  // cache intentionally not maintained
    const std::size_t expected_bid_count =
        std::min(bids_.size(), kDepthLevels);
    if (bid_cache_count_ != expected_bid_count) return false;
    {
        std::size_t i = 0;
        for (auto it = bids_.begin();
             it != bids_.end() && i < expected_bid_count; ++it, ++i) {
            if (cached_bids_[i] != it->second.get()) return false;
        }
    }
    const std::size_t expected_ask_count =
        std::min(asks_.size(), kDepthLevels);
    if (ask_cache_count_ != expected_ask_count) return false;
    {
        std::size_t i = 0;
        for (auto it = asks_.begin();
             it != asks_.end() && i < expected_ask_count; ++it, ++i) {
            if (cached_asks_[i] != it->second.get()) return false;
        }
    }
    return true;
}

}  // namespace meridian
