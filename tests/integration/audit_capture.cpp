// Single-symbol audit capture harness.
//
// Standalone tool that runs each single-symbol audit case through the C++
// MatchingEngine and prints one JSON Lines block per case to stdout. The
// matching-semantics.md case names label each block so the output can be
// pasted into docs/risk/audit-cases.md verbatim.
//
// This file is intentionally not linked into the test suite; it exists as
// a one-shot capture tool that backs the audit document at
// docs/risk/audit-cases.md (section 12 documents how to build and run it).
// The parameterized integration test
// (tests/integration/test_engine_vs_reference.cpp) is the comprehensive
// CI gate and covers all 26 worked examples by name. This capture tool
// exists so the audit document can carry verbatim JSON Lines outputs
// without depending on gtest's per-case formatting.
//
// Build:
//   g++ -std=c++20 -O2 -Iinclude tests/integration/audit_capture.cpp
//       build/src/libmeridian.a -o build/bin/audit_capture
// (single command; line wrapped in source comment for readability).
//
// Run:
//   build/bin/audit_capture

#include "meridian/book.hpp"
#include "meridian/execution_report.hpp"
#include "meridian/matching.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace meridian {
namespace {

struct ScenarioEvent {
    std::string kind;
    std::string type;
    OrderId id;
    std::string side;
    Price price;
    Quantity qty;
    Timestamp ts;
};

std::string report_to_jsonl(const ExecutionReport& r) {
    const char* kind = "ack";
    switch (r.kind) {
        case ReportKind::Acknowledge: kind = "ack"; break;
        case ReportKind::Fill:        kind = "fill"; break;
        case ReportKind::Cancel:      kind = "cancel"; break;
        case ReportKind::Reject:      kind = "reject"; break;
    }
    const char* side = (r.side == Side::Buy) ? "buy" : "sell";
    const char* reason = "none";
    switch (r.reject_reason) {
        case RejectReason::None:                  reason = "none"; break;
        case RejectReason::EmptyBook:             reason = "empty_book"; break;
        case RejectReason::NotFound:              reason = "not_found"; break;
        case RejectReason::InsufficientLiquidity: reason = "insufficient_liquidity"; break;
        case RejectReason::WouldCross:            reason = "would_cross"; break;
    }
    std::ostringstream ss;
    ss << "{"
       << "\"kind\":\"" << kind << "\","
       << "\"order_id\":" << r.order_id << ","
       << "\"price\":" << r.price << ","
       << "\"qty\":" << r.qty << ","
       << "\"reject_reason\":\"" << reason << "\","
       << "\"side\":\"" << side << "\","
       << "\"ts\":" << r.ts
       << "}";
    return ss.str();
}

EngineEvent to_engine_event(const ScenarioEvent& e) {
    EngineEvent ev{};
    ev.symbol = 1;
    ev.ts = e.ts;
    ev.order_id = e.id;
    ev.side = (e.side == "buy") ? Side::Buy : Side::Sell;
    if (e.kind == "cancel") {
        ev.kind = EventKind::Cancel;
    } else {
        ev.kind = EventKind::NewOrder;
        if (e.type == "limit")       ev.type = OrderType::Limit;
        else if (e.type == "market") ev.type = OrderType::Market;
        else if (e.type == "ioc")    ev.type = OrderType::IOC;
        ev.price = e.price;
        ev.qty = e.qty;
    }
    return ev;
}

void run_case(const std::string& name,
              const std::vector<ScenarioEvent>& events) {
    OrderPool pool(1024);
    Book book{1};
    MatchingEngine engine{pool, book};
    std::cout << "=== " << name << " ===\n";
    for (const auto& e : events) {
        auto reports = engine.apply(to_engine_event(e));
        for (const auto& r : reports) {
            std::cout << report_to_jsonl(r) << "\n";
        }
    }
    std::cout << "\n";
}

}  // namespace
}  // namespace meridian

int main() {
    using meridian::ScenarioEvent;
    using meridian::run_case;

    // LIM-3: aggressive limit fully crossing one resting order.
    // Initial: 201 sell at 10001 qty 40. Aggressor 301 buy at 10001 qty 40.
    run_case("LIM-3", {
        {"new_order", "limit", 201, "sell", 10001, 40, 2},
        {"new_order", "limit", 301, "buy",  10001, 40, 3},
    });

    // LIM-4: aggressive limit sweeping two price levels with residual that rests.
    // Initial: 201 sell 10001x30, 202 sell 10002x40, 203 sell 10003x50.
    // Aggressor 302 buy at 10002 qty 100.
    run_case("LIM-4", {
        {"new_order", "limit", 201, "sell", 10001, 30, 1},
        {"new_order", "limit", 202, "sell", 10002, 40, 2},
        {"new_order", "limit", 203, "sell", 10003, 50, 3},
        {"new_order", "limit", 302, "buy",  10002, 100, 4},
    });

    // LIM-5: time priority within a level.
    // Initial: three sells 201, 202, 203 each qty 20 at 10001 (in arrival order).
    // Aggressor 401 buy at 10001 qty 35.
    run_case("LIM-5", {
        {"new_order", "limit", 201, "sell", 10001, 20, 1},
        {"new_order", "limit", 202, "sell", 10001, 20, 2},
        {"new_order", "limit", 203, "sell", 10001, 20, 3},
        {"new_order", "limit", 401, "buy",  10001, 35, 5},
    });

    // MKT-3: market buy partial fill, residual implicitly cancelled.
    // Initial: 201 sell at 10001 qty 30. Aggressor 603 market buy qty 100.
    run_case("MKT-3", {
        {"new_order", "limit",  201, "sell", 10001, 30, 1},
        {"new_order", "market", 603, "buy",  0,     100, 10},
    });

    // MKT-4: market buy against an empty book is rejected (no preceding ack).
    // Initial: empty. Aggressor 604 market buy qty 50.
    run_case("MKT-4", {
        {"new_order", "market", 604, "buy", 0, 50, 11},
    });

    // IOC-2: IOC partial fill, residual implicitly cancelled.
    // Initial: 201 sell at 10001 qty 20. Aggressor 702 IOC buy at 10001 qty 50.
    run_case("IOC-2", {
        {"new_order", "limit", 201, "sell", 10001, 20, 1},
        {"new_order", "ioc",   702, "buy",  10001, 50, 15},
    });

    // IOC-4: IOC at a price that does not cross any resting level (ack-only).
    // Initial: 201 sell at 10005 qty 30. Aggressor 704 IOC buy at 10001 qty 30.
    run_case("IOC-4", {
        {"new_order", "limit", 201, "sell", 10005, 30, 1},
        {"new_order", "ioc",   704, "buy",  10001, 30, 17},
    });

    // CXL-1: cancel a fully resting order (success branch).
    // Initial: 101 buy 10000x50, 102 buy 10000x30. Cancel 101.
    run_case("CXL-1", {
        {"new_order", "limit", 101, "buy", 10000, 50, 1},
        {"new_order", "limit", 102, "buy", 10000, 30, 2},
        {"cancel",    "",      101, "buy", 0,     0,  21},
    });

    // CXL-4: cancel an unknown OrderId (Reject NotFound).
    // Initial: 101 buy 10000x50. Cancel 999 (never registered).
    run_case("CXL-4", {
        {"new_order", "limit", 101, "buy", 10000, 50, 1},
        {"cancel",    "",      999, "buy", 0,     0,  24},
    });

    // CXL-5: cancel an OrderId that was already fully filled (Reject NotFound).
    // Earlier history: 201 sells 10001x30, 301 buys 10001x30 (full cross).
    // 201 is no longer in the lookup map. Cancel 201.
    run_case("CXL-5", {
        {"new_order", "limit", 201, "sell", 10001, 30, 1},
        {"new_order", "limit", 301, "buy",  10001, 30, 2},
        {"cancel",    "",      201, "buy",  0,     0,  25},
    });

    return 0;
}
