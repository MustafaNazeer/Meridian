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

MatchingEngine::MatchingEngine(OrderPool& pool, Book& book) noexcept
    : pool_(pool), book_(book) {}

std::vector<ExecutionReport> MatchingEngine::apply(const EngineEvent& event) {
    std::vector<ExecutionReport> out;
    // Reserve generously so the inner sweep loop never reallocates inside
    // HotPathGuard's scope. Each match emits 2 Fill reports plus an
    // Acknowledge; a sweep that walks 500 makers (worst case for the
    // Phase 1 corner-case scenarios) emits ~1001 reports. The reserve
    // happens outside the HotPathGuard so the initial allocation is
    // legitimate. Phase 5 will replace this with a thread-local fixed-
    // capacity buffer once the bench harness measures the cost.
    out.reserve(1024);
    if (event.kind == EventKind::NewOrder) {
        switch (event.type) {
            case OrderType::Limit:  apply_limit(event,  out); break;
            case OrderType::Market: apply_market(event, out); break;
            case OrderType::IOC:    apply_ioc(event,    out); break;
            case OrderType::PostOnly:
            case OrderType::FOK:
                // Phase 7. For Phase 1 we reject explicitly so a stray
                // PostOnly/FOK does not silently fall through.
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

Quantity MatchingEngine::sweep(Side aggressor_side, Price limit_price,
                               OrderId aggressor_id, Quantity remaining,
                               Timestamp ts,
                               std::vector<ExecutionReport>& out) {
    HotPathGuard guard;
    while (remaining > 0) {
        Level* level = (aggressor_side == Side::Buy)
                           ? book_.best_ask()
                           : book_.best_bid();
        if (level == nullptr) [[unlikely]] {
            break;
        }
        if (!crosses(aggressor_side, limit_price, level->price())) {
            break;
        }

        Order* resting = level->head();
        while (resting != nullptr && remaining > 0) {
            Quantity match_qty = std::min(remaining, resting->qty_remaining);
            // Per matching-semantics.md: each match emits TWO Fill reports
            // back to back, maker first, then taker. Each report is self-
            // describing about its own side (no matched_id field).
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
                Order* removed = book_.remove_by_id(resting->id);
                pool_.release(removed);
            } else {
                level->adjust_qty(resting, resting->qty_remaining - match_qty);
            }
            resting = next;
        }
    }
    return remaining;
}

void MatchingEngine::apply_limit(const EngineEvent& event,
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
    Quantity remaining = sweep(event.side, event.price, event.order_id,
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
        book_.add(resting);
    }
}

void MatchingEngine::apply_market(const EngineEvent& event,
                                  std::vector<ExecutionReport>& out) {
    Level* opposite = (event.side == Side::Buy) ? book_.best_ask() : book_.best_bid();
    if (opposite == nullptr) [[unlikely]] {
        // Per matching-semantics.md section 4.1: market against empty book
        // is a single Reject with no preceding Acknowledge.
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
    sweep(event.side, worst_price, event.order_id, event.qty, event.ts, out);
    // Market orders never rest; any residual is implicitly cancelled.
}

void MatchingEngine::apply_ioc(const EngineEvent& event,
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
    sweep(event.side, event.price, event.order_id, event.qty, event.ts, out);
    // IOC orders never rest; any residual is implicitly cancelled.
}

void MatchingEngine::apply_cancel(const EngineEvent& event,
                                  std::vector<ExecutionReport>& out) {
    Order* removed = book_.remove_by_id(event.order_id);
    if (removed == nullptr) [[unlikely]] {
        // Per matching-semantics.md section 8.2: when the cancel target is
        // unknown, the Reject report carries Side::Buy and price=0 as
        // sentinels (the engine has no way to know the actual side or
        // price). Side::Buy is the zero value of the enum, the natural
        // sentinel without adding a new enumerator.
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
    Quantity cancelled_qty = removed->qty_remaining;
    Side cancelled_side = removed->side;
    Price cancelled_price = removed->price;
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
}

}  // namespace meridian
