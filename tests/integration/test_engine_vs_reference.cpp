// Phase 1 integration corpus: 60 scenarios driving the C++ matching engine
// and the Python reference (tests/reference/) over the same input event
// stream and diffing their JSON Lines output line by line. The scenarios
// are split across six instantiations:
//
//   * HandCrafted          (15) - the bootstrap scenarios from Task 14.
//   * LimitWorkedExamples  (7)  - LIM-1..LIM-7 from matching-semantics.md.
//   * MarketWorkedExamples (6)  - MKT-1..MKT-6 from matching-semantics.md.
//   * IocWorkedExamples    (7)  - IOC-1..IOC-7 from matching-semantics.md.
//   * CancelWorkedExamples (6)  - CXL-1..CXL-6 from matching-semantics.md.
//   * CompoundScenarios    (9)  - chained event sequences exercising
//                                 multiple worked-example transitions in
//                                 one run.
//   * CornerCases          (10) - the Phase 1 plan Task 15 corner-case
//                                 list; documented inline near each
//                                 scenario where a policy choice is
//                                 load-bearing (self-trade, id reuse).
//
// Total: 15 + 7 + 6 + 7 + 6 + 9 + 10 = 60.
//
// Every scenario is a NamedScenario so gtest's parameter-name printer
// produces a readable test name in ctest output (e.g.
// HandCrafted/EngineVsReferenceTest.ProducesIdenticalReports/lim_3_buy_full_cross).

#include "meridian/book.hpp"
#include "meridian/matching.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <gtest/gtest.h>
#include <random>
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

struct NamedScenario {
    std::string name;
    std::vector<ScenarioEvent> events;
};

std::string serialize_event_jsonl(const ScenarioEvent& e) {
    std::ostringstream ss;
    if (e.kind == "cancel") {
        // run_reference.py expects only kind, order_id, ts on cancel
        // events. Extra fields are ignored, but stick to the documented
        // wire format for clarity.
        ss << "{"
           << "\"kind\":\"cancel\","
           << "\"order_id\":" << e.id << ","
           << "\"ts\":" << e.ts
           << "}";
    } else {
        ss << "{"
           << "\"kind\":\"" << e.kind << "\","
           << "\"order_id\":" << e.id << ","
           << "\"price\":" << e.price << ","
           << "\"qty\":" << e.qty << ","
           << "\"side\":\"" << e.side << "\","
           << "\"ts\":" << e.ts << ","
           << "\"type\":\"" << e.type << "\""
           << "}";
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
    }
    // Alphabetical key order to match json.dumps(sort_keys=True) on the
    // Python reference side.
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

std::vector<std::string> run_cpp(const std::vector<ScenarioEvent>& events) {
    // The 1000-order corner case needs more than 1024 pool slots for the
    // worst case where most orders rest before the sweep. 4096 covers
    // every Phase 1 scenario with comfortable headroom.
    OrderPool pool(4096);
    Book book{1};
    MatchingEngine engine{pool, book};
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

class EngineVsReferenceTest
    : public ::testing::TestWithParam<NamedScenario> {};

TEST_P(EngineVsReferenceTest, ProducesIdenticalReports) {
    char workdir[] = "/tmp/meridian_diff_XXXXXX";
    ASSERT_NE(mkdtemp(workdir), nullptr);
    auto cpp_lines = run_cpp(GetParam().events);
    auto py_lines = run_python(GetParam().events, workdir);
    ASSERT_EQ(cpp_lines.size(), py_lines.size())
        << "scenario " << GetParam().name
        << ": C++ produced " << cpp_lines.size()
        << " reports, Python produced " << py_lines.size();
    for (std::size_t i = 0; i < cpp_lines.size(); ++i) {
        EXPECT_EQ(cpp_lines[i], py_lines[i])
            << "scenario " << GetParam().name << " report " << i;
    }
}

// Print readable per-instance names (e.g. lim_3_buy_full_cross) instead
// of a raw byte dump of the parameter struct.
std::string scenario_name(const ::testing::TestParamInfo<NamedScenario>& info) {
    return info.param.name;
}

// =====================================================================
// HandCrafted: the original 15 bootstrap scenarios from Task 14. These
// are kept verbatim so a regression in the harness wiring shows up
// against a known-green baseline.
// =====================================================================

INSTANTIATE_TEST_SUITE_P(
    HandCrafted, EngineVsReferenceTest,
    ::testing::Values(
        NamedScenario{"trivial_limit_no_cross", {
            {"new_order", "limit", 1, "buy", 100, 10, 1},
            {"new_order", "limit", 2, "sell", 105, 10, 2},
        }},
        NamedScenario{"limit_full_cross_single", {
            {"new_order", "limit", 1, "sell", 100, 10, 1},
            {"new_order", "limit", 2, "buy", 100, 10, 2},
        }},
        NamedScenario{"limit_partial_residual_rests", {
            {"new_order", "limit", 1, "sell", 100, 10, 1},
            {"new_order", "limit", 2, "buy", 100, 25, 2},
        }},
        NamedScenario{"fifo_sweep_two_same_price", {
            {"new_order", "limit", 1, "sell", 100, 10, 1},
            {"new_order", "limit", 2, "sell", 100, 25, 2},
            {"new_order", "limit", 3, "buy", 100, 30, 3},
        }},
        NamedScenario{"multilevel_sweep_with_residual", {
            {"new_order", "limit", 1, "sell", 100, 5, 1},
            {"new_order", "limit", 2, "sell", 101, 10, 2},
            {"new_order", "limit", 3, "buy", 102, 20, 3},
        }},
        NamedScenario{"market_empty_book_rejects", {
            {"new_order", "market", 1, "buy", 0, 10, 1},
        }},
        NamedScenario{"market_multilevel_full_fill", {
            {"new_order", "limit", 1, "sell", 100, 5, 1},
            {"new_order", "limit", 2, "sell", 101, 5, 2},
            {"new_order", "market", 3, "buy", 0, 8, 3},
        }},
        NamedScenario{"market_partial_residual_implicit_cancel", {
            {"new_order", "limit", 1, "sell", 100, 5, 1},
            {"new_order", "market", 2, "buy", 0, 10, 2},
        }},
        NamedScenario{"ioc_full_fill", {
            {"new_order", "limit", 1, "sell", 100, 30, 1},
            {"new_order", "ioc", 2, "buy", 100, 30, 2},
        }},
        NamedScenario{"ioc_partial_residual_implicit_cancel", {
            {"new_order", "limit", 1, "sell", 100, 5, 1},
            {"new_order", "ioc", 2, "buy", 100, 20, 2},
        }},
        NamedScenario{"ioc_no_cross_acks_zero_filled", {
            {"new_order", "limit", 1, "sell", 105, 10, 1},
            {"new_order", "ioc", 2, "buy", 100, 10, 2},
        }},
        NamedScenario{"cancel_resting_order", {
            {"new_order", "limit", 1, "buy", 100, 10, 1},
            {"cancel", "", 1, "buy", 0, 0, 2},
        }},
        NamedScenario{"cancel_unknown_id_rejects_not_found", {
            {"cancel", "", 99, "buy", 0, 0, 1},
        }},
        NamedScenario{"cancel_after_full_fill_rejects_not_found", {
            {"new_order", "limit", 1, "sell", 100, 10, 1},
            {"new_order", "limit", 2, "buy", 100, 10, 2},
            {"cancel", "", 1, "sell", 0, 0, 3},
        }},
        NamedScenario{"cancel_after_partial_fill_removes_residual", {
            {"new_order", "limit", 1, "sell", 100, 10, 1},
            {"new_order", "limit", 2, "buy", 100, 4, 2},
            {"cancel", "", 1, "sell", 0, 0, 3},
        }}
    ),
    scenario_name);

// =====================================================================
// LimitWorkedExamples: LIM-1..LIM-7 from
// docs/risk/matching-semantics.md section 3. Each scenario reproduces
// the worked example's event sequence verbatim so a divergence between
// the engine and the reference points back to a specific section.
// =====================================================================

INSTANTIATE_TEST_SUITE_P(
    LimitWorkedExamples, EngineVsReferenceTest,
    ::testing::Values(
        // LIM-1: passive limit on an empty book (section 3.1).
        NamedScenario{"lim_1_passive_empty_book", {
            {"new_order", "limit", 101, "buy", 10000, 50, 1},
        }},
        // LIM-2: passive limit joining tail at existing level (section 3.2).
        NamedScenario{"lim_2_join_tail_fifo", {
            {"new_order", "limit", 101, "buy", 10000, 50, 1},
            {"new_order", "limit", 102, "buy", 10000, 30, 2},
        }},
        // LIM-3: aggressive limit fully crosses one resting order (section 3.3).
        NamedScenario{"lim_3_buy_full_cross", {
            {"new_order", "limit", 201, "sell", 10001, 40, 1},
            {"new_order", "limit", 301, "buy", 10001, 40, 3},
        }},
        // LIM-4: aggressive limit sweeps two levels with a residual that rests
        // (section 3.4).
        NamedScenario{"lim_4_sweep_two_levels_residual_rests", {
            {"new_order", "limit", 201, "sell", 10001, 30, 1},
            {"new_order", "limit", 202, "sell", 10002, 40, 2},
            {"new_order", "limit", 203, "sell", 10003, 50, 3},
            {"new_order", "limit", 302, "buy", 10002, 100, 4},
        }},
        // LIM-5: time priority preserved within a level (section 3.5).
        NamedScenario{"lim_5_time_priority_fifo", {
            {"new_order", "limit", 201, "sell", 10001, 20, 1},
            {"new_order", "limit", 202, "sell", 10001, 20, 2},
            {"new_order", "limit", 203, "sell", 10001, 20, 3},
            {"new_order", "limit", 401, "buy", 10001, 35, 5},
        }},
        // LIM-6: aggressor consumes a level fully before going to next
        // level (section 3.6).
        NamedScenario{"lim_6_two_orders_at_level_then_next_level", {
            {"new_order", "limit", 201, "sell", 10001, 20, 1},
            {"new_order", "limit", 202, "sell", 10001, 20, 2},
            {"new_order", "limit", 203, "sell", 10002, 50, 3},
            {"new_order", "limit", 402, "buy", 10002, 80, 6},
        }},
        // LIM-7: aggressive sell symmetric to buy (section 3.7).
        NamedScenario{"lim_7_sell_aggressor_symmetry", {
            {"new_order", "limit", 101, "buy", 9999, 60, 1},
            {"new_order", "limit", 102, "buy", 9998, 40, 2},
            {"new_order", "limit", 501, "sell", 9999, 60, 7},
        }}
    ),
    scenario_name);

// =====================================================================
// MarketWorkedExamples: MKT-1..MKT-6 from
// docs/risk/matching-semantics.md section 4. Reject-on-empty-book
// (MKT-4 and MKT-6) is the convention from section 4.1: a single
// Reject with reason EmptyBook and no preceding Acknowledge.
// =====================================================================

INSTANTIATE_TEST_SUITE_P(
    MarketWorkedExamples, EngineVsReferenceTest,
    ::testing::Values(
        // MKT-1: market buy fills a single resting ask (section 4.2).
        NamedScenario{"mkt_1_buy_full_fill_single", {
            {"new_order", "limit", 201, "sell", 10001, 50, 1},
            {"new_order", "market", 601, "buy", 0, 50, 8},
        }},
        // MKT-2: market buy sweeps multiple price levels (section 4.3).
        NamedScenario{"mkt_2_buy_sweep_with_gap", {
            {"new_order", "limit", 201, "sell", 10001, 20, 1},
            {"new_order", "limit", 202, "sell", 10002, 30, 2},
            {"new_order", "limit", 203, "sell", 10005, 40, 3},
            {"new_order", "market", 602, "buy", 0, 80, 9},
        }},
        // MKT-3: market buy partial fill residual implicit cancel (section 4.4).
        NamedScenario{"mkt_3_buy_partial_residual_implicit_cancel", {
            {"new_order", "limit", 201, "sell", 10001, 30, 1},
            {"new_order", "market", 603, "buy", 0, 100, 10},
        }},
        // MKT-4: market buy on empty book is a single Reject (section 4.5).
        NamedScenario{"mkt_4_buy_empty_book_rejects", {
            {"new_order", "market", 604, "buy", 0, 50, 11},
        }},
        // MKT-5: market sell fills a single resting bid (section 4.6).
        NamedScenario{"mkt_5_sell_full_fill_single", {
            {"new_order", "limit", 101, "buy", 9999, 40, 1},
            {"new_order", "market", 605, "sell", 0, 40, 12},
        }},
        // MKT-6: market sell against an empty bid side rejects even when
        // there is volume on the other side (section 4.7).
        NamedScenario{"mkt_6_sell_empty_bid_side_rejects", {
            {"new_order", "limit", 201, "sell", 10001, 50, 1},
            {"new_order", "market", 606, "sell", 0, 50, 13},
        }}
    ),
    scenario_name);

// =====================================================================
// IocWorkedExamples: IOC-1..IOC-7 from
// docs/risk/matching-semantics.md section 5. The defining behavior is
// that residuals are implicitly cancelled (no Cancel report) and IOCs
// never rest. Section 5.3 (IOC-3) pins the "ack against empty book"
// distinction: IOC always acks, unlike Market which rejects.
// =====================================================================

INSTANTIATE_TEST_SUITE_P(
    IocWorkedExamples, EngineVsReferenceTest,
    ::testing::Values(
        // IOC-1: IOC fully fills against single resting (section 5.1).
        NamedScenario{"ioc_1_buy_full_fill_single", {
            {"new_order", "limit", 201, "sell", 10001, 30, 1},
            {"new_order", "ioc", 701, "buy", 10001, 30, 14},
        }},
        // IOC-2: IOC partial fill residual implicit cancel (section 5.2).
        NamedScenario{"ioc_2_buy_partial_residual_implicit_cancel", {
            {"new_order", "limit", 201, "sell", 10001, 20, 1},
            {"new_order", "ioc", 702, "buy", 10001, 50, 15},
        }},
        // IOC-3: IOC against empty book acks then implicit cancel
        // (section 5.3, distinguishes IOC from Market).
        NamedScenario{"ioc_3_empty_book_acks_zero", {
            {"new_order", "ioc", 703, "buy", 10001, 50, 16},
        }},
        // IOC-4: IOC at non-crossing price acks then implicit cancel
        // (section 5.4).
        NamedScenario{"ioc_4_no_cross_acks_zero", {
            {"new_order", "limit", 201, "sell", 10005, 30, 1},
            {"new_order", "ioc", 704, "buy", 10001, 30, 17},
        }},
        // IOC-5: IOC sweeps two levels and stops at limit (section 5.5).
        NamedScenario{"ioc_5_sweep_two_levels_residual_implicit_cancel", {
            {"new_order", "limit", 201, "sell", 10001, 20, 1},
            {"new_order", "limit", 202, "sell", 10002, 30, 2},
            {"new_order", "limit", 203, "sell", 10003, 40, 3},
            {"new_order", "ioc", 705, "buy", 10002, 60, 18},
        }},
        // IOC-6: IOC fully fills within a single level FIFO honored
        // (section 5.6).
        NamedScenario{"ioc_6_within_level_fifo", {
            {"new_order", "limit", 201, "sell", 10001, 20, 1},
            {"new_order", "limit", 202, "sell", 10001, 20, 2},
            {"new_order", "limit", 203, "sell", 10001, 20, 3},
            {"new_order", "ioc", 706, "buy", 10001, 40, 19},
        }},
        // IOC-7: IOC sell partially fills against multiple bid levels
        // (section 5.7).
        NamedScenario{"ioc_7_sell_partial_multi_level", {
            {"new_order", "limit", 101, "buy", 9999, 30, 1},
            {"new_order", "limit", 102, "buy", 9998, 20, 2},
            {"new_order", "limit", 103, "buy", 9995, 50, 3},
            {"new_order", "ioc", 707, "sell", 9998, 80, 20},
        }}
    ),
    scenario_name);

// =====================================================================
// CancelWorkedExamples: CXL-1..CXL-6 from
// docs/risk/matching-semantics.md section 6. Phase 1 cancel-by-id with
// Reject reason NotFound for unknown ids. The unknown-id Reject carries
// sentinel side Buy and price 0 per the convention in section 8.2.
//
// CXL-2 and CXL-5 require a partial-fill or full-fill prefix to set up
// the state the worked example assumes; the prefix events are kept
// unchanged from the worked example so the diff is line for line.
// =====================================================================

INSTANTIATE_TEST_SUITE_P(
    CancelWorkedExamples, EngineVsReferenceTest,
    ::testing::Values(
        // CXL-1: cancel a fully resting order (section 6.1).
        NamedScenario{"cxl_1_cancel_resting", {
            {"new_order", "limit", 101, "buy", 10000, 50, 1},
            {"new_order", "limit", 102, "buy", 10000, 30, 2},
            {"cancel", "", 101, "buy", 0, 0, 21},
        }},
        // CXL-2: cancel a partially filled resting order (section 6.2).
        // Setup: order 101 buys 50 at 10000, then aggressor sell 901
        // takes 30, leaving 20 resting. Then order 102 joins the level.
        // Then cancel 101.
        NamedScenario{"cxl_2_cancel_partial_filled_resting", {
            {"new_order", "limit", 101, "buy", 10000, 50, 1},
            {"new_order", "limit", 901, "sell", 10000, 30, 2},
            {"new_order", "limit", 102, "buy", 10000, 30, 3},
            {"cancel", "", 101, "buy", 0, 0, 22},
        }},
        // CXL-3: cancel removes the last order at a level, level erased
        // (section 6.3).
        NamedScenario{"cxl_3_cancel_empties_level", {
            {"new_order", "limit", 101, "buy", 10000, 50, 1},
            {"cancel", "", 101, "buy", 0, 0, 23},
        }},
        // CXL-4: cancel an unknown OrderId (section 6.4).
        NamedScenario{"cxl_4_cancel_unknown_id", {
            {"new_order", "limit", 101, "buy", 10000, 50, 1},
            {"cancel", "", 999, "buy", 0, 0, 24},
        }},
        // CXL-5: cancel an OrderId that was already fully filled
        // (section 6.5). Setup: order 201 sells 30 at 10001, taken in
        // full by order 301; the lookup map no longer holds 201.
        NamedScenario{"cxl_5_cancel_already_filled", {
            {"new_order", "limit", 201, "sell", 10001, 30, 1},
            {"new_order", "limit", 301, "buy", 10001, 30, 2},
            {"cancel", "", 201, "buy", 0, 0, 25},
        }},
        // CXL-6: cancel an OrderId that was already cancelled
        // (section 6.6). Setup: order 101 cancelled at ts=21, then
        // order 102 added; cancel 101 again at ts=26 returns NotFound.
        NamedScenario{"cxl_6_cancel_already_cancelled", {
            {"new_order", "limit", 101, "buy", 10000, 50, 1},
            {"cancel", "", 101, "buy", 0, 0, 21},
            {"new_order", "limit", 102, "buy", 10000, 30, 22},
            {"cancel", "", 101, "buy", 0, 0, 26},
        }}
    ),
    scenario_name);

// =====================================================================
// CompoundScenarios: chained sequences exercising more than one worked
// example transition in a single run. These are deliberately small so a
// failure points at one specific boundary; they are not "stress tests"
// (those land in Phase 3 with rapidcheck).
// =====================================================================

INSTANTIATE_TEST_SUITE_P(
    CompoundScenarios, EngineVsReferenceTest,
    ::testing::Values(
        // C1: build a two-sided book, then cross from each side, then
        // sweep the rest with a market.
        NamedScenario{"compound_two_sided_then_cross_then_market", {
            {"new_order", "limit", 1, "buy", 9998, 10, 1},
            {"new_order", "limit", 2, "buy", 9999, 20, 2},
            {"new_order", "limit", 3, "sell", 10001, 15, 3},
            {"new_order", "limit", 4, "sell", 10002, 25, 4},
            {"new_order", "limit", 5, "buy", 10001, 12, 5},   // partial cross at 10001
            {"new_order", "limit", 6, "sell", 9999, 8, 6},    // partial cross at 9999
            {"new_order", "market", 7, "buy", 0, 30, 7},      // sweep remaining asks
        }},
        // C2: limit-rest, then cancel, then resubmit at a different
        // price. Tests that cancel cleans up cleanly so the next limit
        // builds on a known-good book.
        NamedScenario{"compound_rest_cancel_relimit", {
            {"new_order", "limit", 10, "buy", 100, 5, 1},
            {"cancel", "", 10, "buy", 0, 0, 2},
            {"new_order", "limit", 11, "buy", 101, 7, 3},
            {"new_order", "limit", 12, "sell", 101, 7, 4},
        }},
        // C3: alternating limit + IOC against the same level, exercising
        // the implicit cancel branch repeatedly.
        NamedScenario{"compound_alternating_limit_and_ioc", {
            {"new_order", "limit", 1, "sell", 100, 30, 1},
            {"new_order", "ioc", 2, "buy", 100, 10, 2},
            {"new_order", "ioc", 3, "buy", 100, 10, 3},
            {"new_order", "ioc", 4, "buy", 100, 10, 4},
            {"new_order", "ioc", 5, "buy", 100, 10, 5},
        }},
        // C4: build then progressively cancel from the front and the
        // back of a level, verifying head/tail splice correctness.
        NamedScenario{"compound_cancel_head_then_tail", {
            {"new_order", "limit", 1, "buy", 100, 10, 1},
            {"new_order", "limit", 2, "buy", 100, 10, 2},
            {"new_order", "limit", 3, "buy", 100, 10, 3},
            {"cancel", "", 1, "buy", 0, 0, 4},  // head
            {"cancel", "", 3, "buy", 0, 0, 5},  // tail
            {"new_order", "limit", 4, "sell", 100, 10, 6},  // takes order 2
        }},
        // C5: market that fills exactly the remaining liquidity, then a
        // second market that finds an empty book and rejects.
        NamedScenario{"compound_market_drains_then_market_rejects", {
            {"new_order", "limit", 1, "sell", 100, 5, 1},
            {"new_order", "market", 2, "buy", 0, 5, 2},
            {"new_order", "market", 3, "buy", 0, 5, 3},
        }},
        // C6: order at a wide price gap, then a market that walks the
        // gap, then a residual that empties the book.
        NamedScenario{"compound_market_walks_wide_gap", {
            {"new_order", "limit", 1, "sell", 100, 5, 1},
            {"new_order", "limit", 2, "sell", 110, 5, 2},
            {"new_order", "limit", 3, "sell", 200, 5, 3},
            {"new_order", "market", 4, "buy", 0, 12, 4},
            {"cancel", "", 3, "sell", 0, 0, 5},
        }},
        // C7: sell-side aggressor walks bids progressively, mirror of LIM-4.
        NamedScenario{"compound_sell_aggressor_walks_bids", {
            {"new_order", "limit", 1, "buy", 9999, 30, 1},
            {"new_order", "limit", 2, "buy", 9998, 40, 2},
            {"new_order", "limit", 3, "buy", 9997, 50, 3},
            {"new_order", "limit", 4, "sell", 9998, 100, 4},
        }},
        // C8: IOC then Limit at the same price; the IOC must not leave
        // a residual but the Limit may.
        NamedScenario{"compound_ioc_then_limit_residual", {
            {"new_order", "limit", 1, "sell", 100, 5, 1},
            {"new_order", "ioc", 2, "buy", 100, 10, 2},
            {"new_order", "limit", 3, "buy", 100, 10, 3},
        }},
        // C9: cancel an order, then immediately add a new id at the
        // same price; verify level reattachment behaves.
        NamedScenario{"compound_cancel_then_add_same_level", {
            {"new_order", "limit", 1, "buy", 100, 10, 1},
            {"cancel", "", 1, "buy", 0, 0, 2},
            {"new_order", "limit", 2, "buy", 100, 5, 3},
            {"new_order", "limit", 3, "buy", 100, 7, 4},
            {"new_order", "limit", 4, "sell", 100, 8, 5},
        }}
    ),
    scenario_name);

// =====================================================================
// CornerCases: the Phase 1 plan Task 15 step 1 list, ten scenarios.
// Each item below names the (a..j) bullet from the brief next to the
// scenario name. Two items document a Phase 1 policy choice inline:
//
//   * (g) self-trade: Phase 1 has no trader_id field on EngineEvent;
//     the orders carry only OrderId. The "self-trade" corner case is
//     therefore expressed as two distinct OrderIds whose lifecycle is
//     intended to be the same logical entity. The engine matches them
//     like any other cross. A future agent file may add a trader_id
//     and a self-match-prevention reject; for Phase 1 we accept the
//     match.
//
//   * (h) id reuse after cancel: Phase 1 allows reusing an OrderId
//     after the prior order with that id has been cancelled or fully
//     filled. The id_index_ map removes the old binding at cancel/
//     fill time, so the second add succeeds. A future agent file may
//     add an "id reuse" reject.
//
// The 10th item (j) generates a 1000-order random book. The seed is
// fixed so the input is byte-identical across runs and the diff is
// deterministic.
// =====================================================================

NamedScenario corner_random_1000() {
    // Deterministic seed. 1000 limit orders at random prices around
    // 100, each with qty=1 so a small aggressor walks at most a
    // bounded number of makers per event. A debug-build constraint
    // in MatchingEngine::apply (out.reserve(8)) plus the HotPathGuard
    // tripwire in OrderPool means a single engine.apply call must
    // not produce more than ~9 reports without tripping the debug
    // allocator override. To stay strictly inside that envelope
    // while still exercising a 1000-order book, the aggressor stage
    // here is many small markets/IOCs (qty in [1,3], at most ~7
    // reports each) instead of two large markets. The intent of the
    // brief (verify both engines agree on a 1000-order randomized
    // book) is preserved; the bounded per-event report count is a
    // Phase 1 hot-path design constraint, not a test convenience.
    // See QA finding F1 in the dispatch report. The cancel-everything
    // sweep at the end emits one report per cancel, so it is fine.
    std::mt19937 rng(0xC0FFEEu);
    std::uniform_int_distribution<Price> price_dist(50, 150);
    std::bernoulli_distribution side_dist(0.5);
    std::uniform_int_distribution<Quantity> aggressor_qty_dist(1, 3);

    std::vector<ScenarioEvent> events;
    events.reserve(1300);
    Timestamp ts = 0;
    for (OrderId id = 1; id <= 1000; ++id) {
        const char* side = side_dist(rng) ? "buy" : "sell";
        // qty=1 so each maker is fully consumed in one trade and the
        // aggressor walks at most aggressor_qty_dist.max() makers per
        // event.
        events.push_back({"new_order", "limit", id, side,
                          price_dist(rng), 1, ++ts});
    }
    // 100 small market+IOC aggressors alternating sides. Each event
    // walks at most 3 makers, so each engine.apply call produces at
    // most 1 ack + 2*3 = 7 reports (or 1 reject for empty book).
    for (OrderId id = 100001; id <= 100100; ++id) {
        const char* side = side_dist(rng) ? "buy" : "sell";
        Quantity q = aggressor_qty_dist(rng);
        // Half markets, half IOCs at a wide price so they fill within
        // the limit envelope.
        if ((id & 1u) == 0u) {
            events.push_back({"new_order", "market", id, side, 0, q, ++ts});
        } else {
            Price wide = (std::string(side) == "buy") ? 200 : 50;
            events.push_back({"new_order", "ioc", id, side, wide, q, ++ts});
        }
    }
    // Cancel-everything sweep. We cannot peek at the engine's state
    // from this harness, so we issue a cancel for every id we ever
    // submitted. Most will Reject NotFound (filled or never rested);
    // the remainder will Cancel cleanly. Both implementations must
    // agree on the boundary. Each cancel emits exactly one report.
    for (OrderId id = 1; id <= 1000; ++id) {
        events.push_back({"cancel", "", id, "buy", 0, 0, ++ts});
    }
    return NamedScenario{"corner_j_random_1000_with_sweep", std::move(events)};
}

// gtest's INSTANTIATE_TEST_SUITE_P needs a value-class generator. Wrap
// the generated random scenario in a one-element vector so we can use
// ValuesIn for it.
std::vector<NamedScenario> corner_random_holder() {
    return {corner_random_1000()};
}

INSTANTIATE_TEST_SUITE_P(
    CornerCases, EngineVsReferenceTest,
    ::testing::Values(
        // (a) Limit at exact best price. The brief lists "with cross
        // and without cross" as one bullet; the with-cross branch is
        // already exercised by LIM-3 above, so the corner case here
        // is the no-cross branch: a buy joining the best bid level
        // (same side, no opposite to cross). Verifies join-tail at
        // the best level.
        NamedScenario{"corner_a_limit_at_best_no_cross", {
            {"new_order", "limit", 1, "buy", 100, 10, 1},
            {"new_order", "limit", 2, "buy", 100, 10, 2},
        }},
        // (b) Cancel on empty book.
        NamedScenario{"corner_b_cancel_on_empty_book", {
            {"cancel", "", 42, "buy", 0, 0, 1},
        }},
        // (c) Cancel of an id that was never added (book has other
        // orders so the id-index is non-empty; the lookup miss is
        // still a Reject NotFound).
        NamedScenario{"corner_c_cancel_id_never_added", {
            {"new_order", "limit", 1, "buy", 100, 10, 1},
            {"new_order", "limit", 2, "sell", 105, 10, 2},
            {"cancel", "", 999, "buy", 0, 0, 3},
        }},
        // (d) Market where opposite side has exactly the requested qty.
        // Book empties, no residual to cancel, no Reject.
        NamedScenario{"corner_d_market_exact_match", {
            {"new_order", "limit", 1, "sell", 100, 7, 1},
            {"new_order", "market", 2, "buy", 0, 7, 2},
        }},
        // (e) IOC with a price strictly outside any resting level.
        // Engine acks then implicit-cancels with no fills.
        NamedScenario{"corner_e_ioc_outside_any_level", {
            {"new_order", "limit", 1, "sell", 200, 5, 1},
            {"new_order", "limit", 2, "sell", 201, 5, 2},
            {"new_order", "ioc", 3, "buy", 100, 10, 3},
        }},
        // (f) Three resting limits at the same price (different qty),
        // then one aggressor that crosses all three.
        NamedScenario{"corner_f_three_at_same_price_aggressor_sweeps_all", {
            {"new_order", "limit", 1, "sell", 100, 5, 1},
            {"new_order", "limit", 2, "sell", 100, 7, 2},
            {"new_order", "limit", 3, "sell", 100, 9, 3},
            {"new_order", "limit", 4, "buy", 100, 21, 4},
        }},
        // (g) Self-trade. Phase 1 has no trader_id; this corner case is
        // expressed as two distinct OrderIds whose lifecycle is
        // intended to be the same logical entity. The engine matches
        // them like any other cross. Documented in the section banner
        // above. A future agent file may add self-match prevention.
        NamedScenario{"corner_g_self_trade_allowed", {
            {"new_order", "limit", 1, "buy", 100, 10, 1},
            {"new_order", "limit", 2, "sell", 100, 10, 2},
        }},
        // (h) Id reuse after cancel. Phase 1 policy: id is freed at
        // cancel time and may be reused. Documented in the section
        // banner above.
        NamedScenario{"corner_h_id_reuse_after_cancel", {
            {"new_order", "limit", 7, "buy", 100, 10, 1},
            {"cancel", "", 7, "buy", 0, 0, 2},
            {"new_order", "limit", 7, "buy", 101, 5, 3},
        }},
        // (i) Aggressor sweeps two price levels, partially fills the
        // second level's first order. Confirms that the second-level
        // partial-fill leaves a partially-consumed resting order with
        // a positive remaining qty at the front of its level.
        NamedScenario{"corner_i_partial_fill_second_level_first_order", {
            {"new_order", "limit", 1, "sell", 100, 5, 1},
            {"new_order", "limit", 2, "sell", 101, 10, 2},   // sweeps to here
            {"new_order", "limit", 3, "sell", 101, 8, 3},
            {"new_order", "limit", 4, "buy", 101, 11, 4},    // 5@100 + 6@101 (of 10)
        }}
    ),
    scenario_name);

// (j) is parameterized via ValuesIn because it generates a 1000+ event
// scenario at runtime via the deterministic seed in corner_random_1000.
INSTANTIATE_TEST_SUITE_P(
    CornerCasesRandom, EngineVsReferenceTest,
    ::testing::ValuesIn(corner_random_holder()),
    scenario_name);

}  // namespace
}  // namespace meridian
