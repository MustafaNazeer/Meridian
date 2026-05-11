#include "meridian/matching.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <limits>

namespace meridian {

HotPathGuard::HotPathGuard() noexcept {
#ifndef NDEBUG
    debug_alloc::hot_path_active.store(true, std::memory_order_relaxed);
#endif
}

HotPathGuard::~HotPathGuard() noexcept {
#ifndef NDEBUG
    debug_alloc::hot_path_active.store(false, std::memory_order_relaxed);
#endif
}

MatchingEngine::MatchingEngine(OrderPool& pool, BookRegistry& registry,
                               OrderIndex& index) noexcept
    : pool_(pool), registry_(registry), index_(index) {}

void MatchingEngine::apply(const EngineEvent& event,
                           std::vector<ExecutionReport>& out) {
    const auto t_start = std::chrono::steady_clock::now();
    const std::size_t reports_before = out.size();
    Book* book = nullptr;

    if (event.kind == EventKind::NewOrder) {
        book = registry_.book(event.symbol);
        if (book == nullptr) [[unlikely]] {
            out.push_back(ExecutionReport{
                .kind = ReportKind::Reject,
                .order_id = event.order_id,
                .side = event.side,
                .price = event.price,
                .qty = event.qty,
                .ts = event.ts,
                .reject_reason = RejectReason::UnknownSymbol,
            });
            latency_.record(std::chrono::steady_clock::now() - t_start);
            return;
        }
        // HotPathGuard remains inside sweep(): it watches the inner
        // matching loop, where any allocation would indicate a bug (a
        // runaway report buffer growth, an accidental temporary). The
        // legitimate allocations on the new-price-level path
        // (`std::map<Price, std::unique_ptr<Level>>` insertion in
        // `Book::add`) are by design and must not trip the debug
        // allocator. Lifting the guard to wrap the whole apply() would
        // false-positive on those legitimate paths.
        switch (event.type) {
            case OrderType::Limit:    apply_limit(event,     *book, out); break;
            case OrderType::Market:   apply_market(event,    *book, out); break;
            case OrderType::IOC:      apply_ioc(event,       *book, out); break;
            case OrderType::PostOnly: apply_post_only(event, *book, out); break;
            case OrderType::FOK:      apply_fok(event,       *book, out); break;
        }
        // Publish post-event top of book for any path that reached an
        // accepted book. PostOnly / FOK reject without touching the
        // book, but republishing the unchanged snapshot is harmless and
        // keeps the publish path uniform; the only added cost is two
        // extra seq stores per rejected event, which is not on any hot
        // measured path.
        book->publish_top_of_book(event.ts);
        book->publish_depth(event.ts);

        // Emit trade prints: walk the reports produced by this event,
        // identify (Fill maker, Fill taker) pairs, and push one
        // TradePrint per pair. The aggressor side is the taker's side
        // (= event.side for NewOrder events). Reports were appended to
        // `out` starting at index `reports_before`.
        const std::size_t end = out.size();
        for (std::size_t i = reports_before; i + 1 < end; ++i) {
            const ExecutionReport& maker = out[i];
            const ExecutionReport& taker = out[i + 1];
            if (maker.kind == ReportKind::Fill &&
                taker.kind == ReportKind::Fill &&
                maker.order_id != taker.order_id &&
                taker.order_id == event.order_id) {
                book->publish_trade(TradePrint{
                    .ts = event.ts,
                    .price = maker.price,
                    .qty = maker.qty,
                    .aggressor = event.side,
                    .seq = 0,  // assigned by Book::publish_trade
                });
                ++i;  // skip past the taker leg we already consumed
            }
        }
    } else {
        apply_cancel(event, out);
        // apply_cancel publishes top_of_book on the success path
        // (src/matching.cpp inside apply_cancel). Mirror the depth
        // publish there so cancels also keep the depth seqlock current.
        // We look up the book here to avoid double-publishing on the
        // NotFound reject path, which apply_cancel does not publish.
        const std::size_t end = out.size();
        if (end > reports_before) {
            const ExecutionReport& last = out[end - 1];
            if (last.kind == ReportKind::Cancel) {
                Book* cb = registry_.book(event.symbol);
                if (cb != nullptr) cb->publish_depth(event.ts);
            }
        }
    }

    latency_.record(std::chrono::steady_clock::now() - t_start);
}

namespace {

inline bool crosses(Side aggressor_side, Price limit_price, Price resting_price) {
    if (aggressor_side == Side::Buy) {
        return resting_price <= limit_price;
    }
    return resting_price >= limit_price;
}

}  // namespace

Quantity MatchingEngine::sweep(Book& book, Side aggressor_side,
                               Price limit_price, OrderId aggressor_id,
                               Quantity remaining, Timestamp ts,
                               std::vector<ExecutionReport>& out) {
    HotPathGuard guard;
    while (remaining > 0) {
        Level* level = (aggressor_side == Side::Buy) ? book.best_ask()
                                                     : book.best_bid();
        if (level == nullptr) [[unlikely]] {
            break;
        }
        if (!crosses(aggressor_side, limit_price, level->price())) {
            break;
        }

        Order* resting = level->head();
        while (resting != nullptr && remaining > 0) {
            Quantity match_qty = std::min(remaining, resting->qty_remaining);
            out.push_back(ExecutionReport{
                .kind = ReportKind::Fill,
                .order_id = resting->id,
                .side = resting->side,
                .price = level->price(),
                .qty = match_qty,
                .ts = ts,
                .reject_reason = RejectReason::None,
            });
            out.push_back(ExecutionReport{
                .kind = ReportKind::Fill,
                .order_id = aggressor_id,
                .side = aggressor_side,
                .price = level->price(),
                .qty = match_qty,
                .ts = ts,
                .reject_reason = RejectReason::None,
            });
            remaining -= match_qty;
            Order* next = resting->next;
            if (match_qty == resting->qty_remaining) {
                OrderId resting_id = resting->id;
                Order* removed = book.remove_by_id(resting_id);
                index_.erase(resting_id);
                pool_.release(removed);
            } else {
                level->adjust_qty(resting, resting->qty_remaining - match_qty);
            }
            resting = next;
        }
    }
    return remaining;
}

void MatchingEngine::apply_limit(const EngineEvent& event, Book& book,
                                 std::vector<ExecutionReport>& out) {
    out.push_back(ExecutionReport{
        .kind = ReportKind::Acknowledge,
        .order_id = event.order_id,
        .side = event.side,
        .price = event.price,
        .qty = event.qty,
        .ts = event.ts,
        .reject_reason = RejectReason::None,
    });
    Quantity remaining = sweep(book, event.side, event.price, event.order_id,
                               event.qty, event.ts, out);
    if (remaining > 0) {
        Order* resting = pool_.acquire();
        resting->id = event.order_id;
        resting->symbol = event.symbol;
        resting->side = event.side;
        resting->type = event.type;
        resting->price = event.price;
        resting->qty_remaining = remaining;
        resting->ts_arrival = event.ts;
        book.add(resting);
        index_.insert(event.order_id, resting);
    }
}

void MatchingEngine::apply_market(const EngineEvent& event, Book& book,
                                  std::vector<ExecutionReport>& out) {
    Level* opposite = (event.side == Side::Buy) ? book.best_ask() : book.best_bid();
    if (opposite == nullptr) [[unlikely]] {
        out.push_back(ExecutionReport{
            .kind = ReportKind::Reject,
            .order_id = event.order_id,
            .side = event.side,
            .price = event.price,
            .qty = event.qty,
            .ts = event.ts,
            .reject_reason = RejectReason::EmptyBook,
        });
        return;
    }
    out.push_back(ExecutionReport{
        .kind = ReportKind::Acknowledge,
        .order_id = event.order_id,
        .side = event.side,
        .price = event.price,
        .qty = event.qty,
        .ts = event.ts,
        .reject_reason = RejectReason::None,
    });
    Price worst_price = (event.side == Side::Buy)
                            ? std::numeric_limits<Price>::max()
                            : std::numeric_limits<Price>::min();
    sweep(book, event.side, worst_price, event.order_id, event.qty, event.ts, out);
}

void MatchingEngine::apply_ioc(const EngineEvent& event, Book& book,
                               std::vector<ExecutionReport>& out) {
    out.push_back(ExecutionReport{
        .kind = ReportKind::Acknowledge,
        .order_id = event.order_id,
        .side = event.side,
        .price = event.price,
        .qty = event.qty,
        .ts = event.ts,
        .reject_reason = RejectReason::None,
    });
    sweep(book, event.side, event.price, event.order_id, event.qty, event.ts, out);
}

void MatchingEngine::apply_post_only(const EngineEvent& event, Book& book,
                                     std::vector<ExecutionReport>& out) {
    // Would the order cross? Buy at price >= best ask, or sell at price
    // <= best bid. Per matching-semantics.md section 6.1, a would-cross
    // post-only order is rejected with reason WouldCross and no
    // preceding Acknowledge (mirroring the Market-against-empty
    // convention from section 4.1).
    Level* opposite = (event.side == Side::Buy) ? book.best_ask() : book.best_bid();
    if (opposite != nullptr && crosses(event.side, event.price, opposite->price())) {
        out.push_back(ExecutionReport{
            .kind = ReportKind::Reject,
            .order_id = event.order_id,
            .side = event.side,
            .price = event.price,
            .qty = event.qty,
            .ts = event.ts,
            .reject_reason = RejectReason::WouldCross,
        });
        return;
    }
    // No cross. Ack and rest at the limit price, like a Limit that
    // doesn't cross (the matching loop is bypassed entirely; nothing to
    // sweep against).
    out.push_back(ExecutionReport{
        .kind = ReportKind::Acknowledge,
        .order_id = event.order_id,
        .side = event.side,
        .price = event.price,
        .qty = event.qty,
        .ts = event.ts,
        .reject_reason = RejectReason::None,
    });
    Order* resting = pool_.acquire();
    resting->id = event.order_id;
    resting->symbol = event.symbol;
    resting->side = event.side;
    resting->type = event.type;
    resting->price = event.price;
    resting->qty_remaining = event.qty;
    resting->ts_arrival = event.ts;
    book.add(resting);
    index_.insert(event.order_id, resting);
}

void MatchingEngine::apply_fok(const EngineEvent& event, Book& book,
                               std::vector<ExecutionReport>& out) {
    // FOK: fill in full, or reject. Per matching-semantics.md section
    // 7.1, an unfillable FOK is rejected with reason
    // InsufficientLiquidity and no preceding Acknowledge.
    //
    // Liquidity scan walks opposite-side levels in price order, summing
    // total_qty() at each crossing level until it reaches the requested
    // quantity or runs out of crossing levels. The scan does not mutate
    // the book; books_ / asks_ access is via the MatchingEngine friend
    // declaration on Book.
    Quantity available = 0;
    if (event.side == Side::Buy) {
        for (const auto& [price, level] : book.asks_) {
            if (price > event.price) break;
            available += level->total_qty();
            if (available >= event.qty) break;
        }
    } else {
        for (const auto& [price, level] : book.bids_) {
            if (price < event.price) break;
            available += level->total_qty();
            if (available >= event.qty) break;
        }
    }
    if (available < event.qty) {
        out.push_back(ExecutionReport{
            .kind = ReportKind::Reject,
            .order_id = event.order_id,
            .side = event.side,
            .price = event.price,
            .qty = event.qty,
            .ts = event.ts,
            .reject_reason = RejectReason::InsufficientLiquidity,
        });
        return;
    }
    // Sufficient liquidity. Ack and fully fill via the matching loop.
    // FOK never rests: by construction, the sweep consumes exactly
    // event.qty here and leaves zero residual.
    out.push_back(ExecutionReport{
        .kind = ReportKind::Acknowledge,
        .order_id = event.order_id,
        .side = event.side,
        .price = event.price,
        .qty = event.qty,
        .ts = event.ts,
        .reject_reason = RejectReason::None,
    });
    sweep(book, event.side, event.price, event.order_id, event.qty, event.ts, out);
}

void MatchingEngine::apply_cancel(const EngineEvent& event,
                                  std::vector<ExecutionReport>& out) {
    Order* order = index_.find(event.order_id);
    if (order == nullptr) [[unlikely]] {
        out.push_back(ExecutionReport{
            .kind = ReportKind::Reject,
            .order_id = event.order_id,
            .side = Side::Buy,
            .price = 0,
            .qty = 0,
            .ts = event.ts,
            .reject_reason = RejectReason::NotFound,
        });
        return;
    }
    Symbol sym = order->symbol;
    Side cancelled_side = order->side;
    Price cancelled_price = order->price;
    Quantity cancelled_qty = order->qty_remaining;
    Book* book = registry_.book(sym);
    // book must exist; the order's symbol was registered when it was placed.
    Order* removed = book->remove_by_id(event.order_id);
    index_.erase(event.order_id);
    pool_.release(removed);
    out.push_back(ExecutionReport{
        .kind = ReportKind::Cancel,
        .order_id = event.order_id,
        .side = cancelled_side,
        .price = cancelled_price,
        .qty = cancelled_qty,
        .ts = event.ts,
        .reject_reason = RejectReason::None,
    });
    // Publish post-cancel top of book. A NotFound cancel returns
    // earlier without touching any book, so no publish is needed on
    // that branch (the snapshot reflects the most recent accepted
    // event, which is the right reading).
    book->publish_top_of_book(event.ts);
}

}  // namespace meridian
