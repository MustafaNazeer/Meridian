#pragma once

#include "meridian/book.hpp"
#include "meridian/book_registry.hpp"
#include "meridian/execution_report.hpp"
#include "meridian/order_index.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <atomic>
#include <vector>

namespace meridian {

namespace debug_alloc {
extern std::atomic<bool> hot_path_active;
}

class HotPathGuard {
public:
    HotPathGuard() noexcept;
    ~HotPathGuard() noexcept;
    HotPathGuard(const HotPathGuard&) = delete;
    HotPathGuard& operator=(const HotPathGuard&) = delete;
};

class MatchingEngine {
public:
    MatchingEngine(OrderPool& pool, BookRegistry& registry,
                   OrderIndex& index) noexcept;

    // Hot-path API: appends every report produced by this event to
    // `out`. Caller owns the vector and is responsible for clear()ing
    // it if they want only this event's reports. The matching loop
    // (`sweep()`) runs under a HotPathGuard so the Debug heap-alloc
    // check catches any accidental allocation in the inner loop.
    // Legitimate allocations on the new-price-level path
    // (`std::map<Price, std::unique_ptr<Level>>` insertion in
    // `Book::add`) are by design and run outside the guard.
    void apply(const EngineEvent& event, std::vector<ExecutionReport>& out);

    // Convenience overload: allocates a fresh vector and forwards to
    // the out-param overload. Used by tests and ad-hoc callers; the
    // bench and any future zero-alloc consumer should call the
    // out-param overload directly.
    std::vector<ExecutionReport> apply(const EngineEvent& event) {
        std::vector<ExecutionReport> out;
        out.reserve(64);
        apply(event, out);
        return out;
    }

private:
    void apply_limit(const EngineEvent& event, Book& book,
                     std::vector<ExecutionReport>& out);
    void apply_market(const EngineEvent& event, Book& book,
                      std::vector<ExecutionReport>& out);
    void apply_ioc(const EngineEvent& event, Book& book,
                   std::vector<ExecutionReport>& out);
    void apply_cancel(const EngineEvent& event,
                      std::vector<ExecutionReport>& out);

    Quantity sweep(Book& book, Side aggressor_side, Price limit_price,
                   OrderId aggressor_id, Quantity remaining,
                   Timestamp ts, std::vector<ExecutionReport>& out);

    OrderPool& pool_;
    BookRegistry& registry_;
    OrderIndex& index_;
};

}  // namespace meridian
