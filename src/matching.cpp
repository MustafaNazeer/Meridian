#include "meridian/matching.hpp"

#include <algorithm>
#include <atomic>
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

std::vector<ExecutionReport> MatchingEngine::apply(const EngineEvent& event) {
    std::vector<ExecutionReport> out;
    out.reserve(1024);
    if (event.kind == EventKind::NewOrder) {
        Book* book = registry_.book(event.symbol);
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
            return out;
        }
        switch (event.type) {
            case OrderType::Limit:  apply_limit(event,  *book, out); break;
            case OrderType::Market: apply_market(event, *book, out); break;
            case OrderType::IOC:    apply_ioc(event,    *book, out); break;
            case OrderType::PostOnly:
            case OrderType::FOK:
                out.push_back(ExecutionReport{
                    .kind = ReportKind::Reject,
                    .order_id = event.order_id,
                    .side = event.side,
                    .price = event.price,
                    .qty = event.qty,
                    .ts = event.ts,
                    .reject_reason = RejectReason::None,
                });
                break;
        }
        // Publish post-event top of book for any path that reached an
        // accepted book. PostOnly / FOK reject without touching the
        // book, but republishing the unchanged snapshot is harmless and
        // keeps the publish path uniform; the only added cost is two
        // extra seq stores per rejected event, which is not on any hot
        // measured path.
        book->publish_top_of_book(event.ts);
    } else {
        apply_cancel(event, out);
    }
    return out;
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
