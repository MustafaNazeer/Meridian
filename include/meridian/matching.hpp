#pragma once

#include "meridian/book.hpp"
#include "meridian/execution_report.hpp"
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
    MatchingEngine(OrderPool& pool, Book& book) noexcept;

    std::vector<ExecutionReport> apply(const EngineEvent& event);

private:
    void apply_limit(const EngineEvent& event,
                     std::vector<ExecutionReport>& out);
    void apply_market(const EngineEvent& event,
                      std::vector<ExecutionReport>& out);
    void apply_ioc(const EngineEvent& event,
                   std::vector<ExecutionReport>& out);
    void apply_cancel(const EngineEvent& event,
                      std::vector<ExecutionReport>& out);

    Quantity sweep(Side aggressor_side, Price limit_price,
                   OrderId aggressor_id, Quantity remaining,
                   Timestamp ts, std::vector<ExecutionReport>& out);

    OrderPool& pool_;
    Book& book_;
};

}  // namespace meridian
