// Multi-instrument integration test. Drives 5 demo symbols
// (AAPL=1, SPY=2, NVDA=3, TSLA=4, GOOG=5) through both the C++ engine
// and the Python reference, byte-diffing the report streams.

#include "meridian/book.hpp"
#include "meridian/book_registry.hpp"
#include "meridian/matching.hpp"
#include "meridian/order_index.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <gtest/gtest.h>
#include <random>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace meridian {
namespace {

struct ScenarioEvent {
    std::string kind;
    std::string type;
    OrderId id;
    Symbol symbol;
    std::string side;
    Price price;
    Quantity qty;
    Timestamp ts;
};

struct NamedScenario {
    std::string name;
    std::vector<ScenarioEvent> events;
};

std::string serialize_event_jsonl(const ScenarioEvent& e) {
    std::ostringstream ss;
    if (e.kind == "cancel") {
        ss << "{\"kind\":\"cancel\","
           << "\"order_id\":" << e.id << ","
           << "\"ts\":" << e.ts << "}";
    } else {
        ss << "{\"kind\":\"" << e.kind << "\","
           << "\"order_id\":" << e.id << ","
           << "\"price\":" << e.price << ","
           << "\"qty\":" << e.qty << ","
           << "\"side\":\"" << e.side << "\","
           << "\"symbol\":" << e.symbol << ","
           << "\"ts\":" << e.ts << ","
           << "\"type\":\"" << e.type << "\"}";
    }
    return ss.str();
}

std::string report_to_jsonl(const ExecutionReport& r) {
    const char* kind = nullptr;
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
        case RejectReason::UnknownSymbol:         reason = "unknown_symbol"; break;
    }
    std::ostringstream ss;
    ss << "{\"kind\":\"" << kind << "\","
       << "\"order_id\":" << r.order_id << ","
       << "\"price\":" << r.price << ","
       << "\"qty\":" << r.qty << ","
       << "\"reject_reason\":\"" << reason << "\","
       << "\"side\":\"" << side << "\","
       << "\"ts\":" << r.ts << "}";
    return ss.str();
}

EngineEvent to_engine_event(const ScenarioEvent& e) {
    EngineEvent ev{};
    ev.symbol = e.symbol;
    ev.ts = e.ts;
    ev.order_id = e.id;
    ev.side = (e.side == "buy") ? Side::Buy : Side::Sell;
    if (e.kind == "cancel") {
        ev.kind = EventKind::Cancel;
    } else {
        ev.kind = EventKind::NewOrder;
        if (e.type == "limit")          ev.type = OrderType::Limit;
        else if (e.type == "market")    ev.type = OrderType::Market;
        else if (e.type == "ioc")       ev.type = OrderType::IOC;
        else if (e.type == "postonly")  ev.type = OrderType::PostOnly;
        else if (e.type == "fok")       ev.type = OrderType::FOK;
        ev.price = e.price;
        ev.qty = e.qty;
    }
    return ev;
}

std::vector<std::string> run_cpp(const std::vector<ScenarioEvent>& events) {
    OrderPool pool(4096);
    BookRegistry registry{1, 2, 3, 4, 5};
    OrderIndex index;
    MatchingEngine engine{pool, registry, index};
    std::vector<std::string> lines;
    for (const auto& e : events) {
        auto reports = engine.apply(to_engine_event(e));
        for (const auto& r : reports) {
            lines.push_back(report_to_jsonl(r));
        }
    }
    return lines;
}

std::vector<std::string> run_python(const std::vector<ScenarioEvent>& events,
                                    const std::string& workdir) {
    std::string input_path = workdir + "/input.jsonl";
    std::string output_path = workdir + "/output.jsonl";
    {
        std::ofstream f(input_path);
        for (const auto& e : events) f << serialize_event_jsonl(e) << "\n";
    }
    std::string cmd = "PYTHONPATH=" SOURCE_REFERENCE_DIR
                      " python3 " SOURCE_REFERENCE_DIR "/run_reference.py"
                      " < " + input_path + " > " + output_path;
    int rc = std::system(cmd.c_str());
    EXPECT_EQ(rc, 0);
    std::ifstream f(output_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line)) lines.push_back(line);
    return lines;
}

class MultiInstrumentTest
    : public ::testing::TestWithParam<NamedScenario> {};

TEST_P(MultiInstrumentTest, ProducesIdenticalReports) {
    char workdir[] = "/tmp/meridian_multi_XXXXXX";
    ASSERT_NE(mkdtemp(workdir), nullptr);
    auto cpp_lines = run_cpp(GetParam().events);
    auto py_lines = run_python(GetParam().events, workdir);
    std::string rm_cmd = "rm -rf " + std::string(workdir);
    [[maybe_unused]] int rm_rc = std::system(rm_cmd.c_str());
    ASSERT_EQ(cpp_lines.size(), py_lines.size())
        << "scenario " << GetParam().name
        << ": C++ produced " << cpp_lines.size()
        << " reports, Python produced " << py_lines.size();
    for (std::size_t i = 0; i < cpp_lines.size(); ++i) {
        EXPECT_EQ(cpp_lines[i], py_lines[i])
            << "scenario " << GetParam().name << " report " << i;
    }
}

std::string scenario_name(const ::testing::TestParamInfo<NamedScenario>& info) {
    return info.param.name;
}

NamedScenario five_symbol_independent_activity() {
    NamedScenario s;
    s.name = "five_symbol_independent";
    Timestamp ts = 1;
    for (Symbol sym = 1; sym <= 5; ++sym) {
        // 3 limits + 1 market per symbol; symbols are independent so each
        // book grows in isolation.
        s.events.push_back({"new_order", "limit", static_cast<OrderId>(sym * 10 + 1),
                            sym, "buy", 100, 5, ts++});
        s.events.push_back({"new_order", "limit", static_cast<OrderId>(sym * 10 + 2),
                            sym, "sell", 105, 5, ts++});
        s.events.push_back({"new_order", "limit", static_cast<OrderId>(sym * 10 + 3),
                            sym, "sell", 106, 10, ts++});
        s.events.push_back({"new_order", "market", static_cast<OrderId>(sym * 10 + 4),
                            sym, "buy", 0, 7, ts++});
    }
    return s;
}

NamedScenario cross_symbol_cancel_isolation() {
    return {"cross_symbol_cancel_isolation", {
        {"new_order", "limit", 100, 1, "buy", 100, 10, 1},   // symbol 1
        {"new_order", "limit", 200, 2, "buy", 100, 10, 2},   // symbol 2
        {"cancel",    "",      100, 0, "buy",   0,  0, 3},   // cancel id 100; engine routes to symbol 1
        // symbol 2's order id 200 should still be resting; place a sell on
        // symbol 2 to verify the order is still there.
        {"new_order", "limit", 201, 2, "sell", 100, 10, 4},
    }};
}

NamedScenario cross_symbol_cancel_by_id_routing() {
    return {"cross_symbol_cancel_by_id_routing", {
        {"new_order", "limit", 1, 1, "buy",  99,  10, 1},
        {"new_order", "limit", 2, 2, "buy",  99,  10, 2},
        {"new_order", "limit", 3, 3, "buy",  99,  10, 3},
        {"new_order", "limit", 4, 4, "buy",  99,  10, 4},
        {"new_order", "limit", 5, 5, "buy",  99,  10, 5},
        // Cancel id=5 without naming the symbol; engine routes via OrderIndex.
        {"cancel",    "",      5, 0, "buy",   0,   0, 6},
        // Place a sell against the bid on symbol 5 to verify symbol 5's
        // book is empty after the cancel.
        {"new_order", "ioc",   6, 5, "sell", 99,  10, 7},
    }};
}

NamedScenario unknown_symbol_reject() {
    return {"unknown_symbol_reject", {
        {"new_order", "limit",  10, 99, "buy",  100, 10, 1},  // unregistered
        {"new_order", "market", 11, 42, "buy",    0, 10, 2},  // unregistered, market
    }};
}

NamedScenario id_reuse_after_cancel_different_symbol() {
    return {"id_reuse_after_cancel_different_symbol", {
        {"new_order", "limit", 7, 1, "buy", 100, 10, 1},
        {"cancel",    "",      7, 0, "buy",   0,  0, 2},
        // Reuse id 7 on a different symbol after cancel.
        {"new_order", "limit", 7, 3, "sell", 200, 5, 3},
    }};
}

NamedScenario cancel_after_fully_filled_multi_symbol() {
    // Audit case: cancel an OrderId that was already fully filled, in
    // a multi-symbol setting. The cross-symbol OrderIndex should miss
    // the lookup and emit Reject NotFound, leaving every book on every
    // symbol unchanged. Mirrors CXL-5 from the single-symbol set but
    // routes through BookRegistry plus OrderIndex.
    return {"cancel_after_fully_filled_multi_symbol", {
        // Symbol 2 maker rests, then is fully consumed by a crossing taker.
        {"new_order", "limit", 201, 2, "sell", 10001, 30, 1},
        {"new_order", "limit", 301, 2, "buy",  10001, 30, 2},
        // Symbol 4 carries an unrelated resting order; it must remain
        // untouched after the cancel-after-fully-filled below.
        {"new_order", "limit", 401, 4, "buy",   9000, 15, 3},
        // Cancel id 201 (already fully filled). Engine must route via
        // OrderIndex, miss, and emit Reject NotFound.
        {"cancel",    "",      201, 0, "buy",      0,  0, 4},
        // Sanity touch on symbol 4: place a sell that crosses 401 to
        // confirm symbol 4's book is intact and routes correctly.
        {"new_order", "ioc",   402, 4, "sell",  9000, 15, 5},
    }};
}

NamedScenario mixed_50_event_corpus() {
    NamedScenario s;
    s.name = "mixed_50_event_random";
    std::mt19937 rng(0xBEEF);
    std::uniform_int_distribution<int> sym_dist(1, 5);
    std::uniform_int_distribution<Price> px_dist(95, 105);
    std::uniform_int_distribution<Quantity> qty_dist(1, 5);
    std::bernoulli_distribution buy_dist(0.5);
    OrderId next_id = 1;
    for (int i = 0; i < 50; ++i) {
        ScenarioEvent ev;
        ev.kind = "new_order";
        ev.type = "limit";
        ev.id = next_id++;
        ev.symbol = static_cast<Symbol>(sym_dist(rng));
        ev.side = buy_dist(rng) ? "buy" : "sell";
        ev.price = px_dist(rng);
        ev.qty = qty_dist(rng);
        ev.ts = i + 1;
        s.events.push_back(ev);
    }
    return s;
}

INSTANTIATE_TEST_SUITE_P(
    MultiInstrument, MultiInstrumentTest,
    ::testing::Values(
        five_symbol_independent_activity(),
        cross_symbol_cancel_isolation(),
        cross_symbol_cancel_by_id_routing(),
        unknown_symbol_reject(),
        id_reuse_after_cancel_different_symbol(),
        cancel_after_fully_filled_multi_symbol(),
        mixed_50_event_corpus()
    ),
    scenario_name);

}  // namespace
}  // namespace meridian
