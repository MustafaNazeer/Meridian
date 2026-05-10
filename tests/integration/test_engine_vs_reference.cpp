#include "meridian/book.hpp"
#include "meridian/matching.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <gtest/gtest.h>
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
    OrderPool pool(1024);
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
    : public ::testing::TestWithParam<std::vector<ScenarioEvent>> {};

TEST_P(EngineVsReferenceTest, ProducesIdenticalReports) {
    char workdir[] = "/tmp/meridian_diff_XXXXXX";
    ASSERT_NE(mkdtemp(workdir), nullptr);
    auto cpp_lines = run_cpp(GetParam());
    auto py_lines = run_python(GetParam(), workdir);
    ASSERT_EQ(cpp_lines.size(), py_lines.size())
        << "C++ produced " << cpp_lines.size()
        << " reports, Python produced " << py_lines.size();
    for (std::size_t i = 0; i < cpp_lines.size(); ++i) {
        EXPECT_EQ(cpp_lines[i], py_lines[i]) << "report " << i;
    }
}

INSTANTIATE_TEST_SUITE_P(
    HandCrafted, EngineVsReferenceTest,
    ::testing::Values(
        // Scenario 1: trivial limit-rest, no cross.
        std::vector<ScenarioEvent>{
            {"new_order", "limit", 1, "buy", 100, 10, 1},
            {"new_order", "limit", 2, "sell", 105, 10, 2},
        },
        // Scenario 2: limit fully crosses single resting order.
        std::vector<ScenarioEvent>{
            {"new_order", "limit", 1, "sell", 100, 10, 1},
            {"new_order", "limit", 2, "buy", 100, 10, 2},
        },
        // Scenario 3: limit partial fill with residual that rests.
        std::vector<ScenarioEvent>{
            {"new_order", "limit", 1, "sell", 100, 10, 1},
            {"new_order", "limit", 2, "buy", 100, 25, 2},
        },
        // Scenario 4: FIFO sweep of two same-price orders.
        std::vector<ScenarioEvent>{
            {"new_order", "limit", 1, "sell", 100, 10, 1},
            {"new_order", "limit", 2, "sell", 100, 25, 2},
            {"new_order", "limit", 3, "buy", 100, 30, 3},
        },
        // Scenario 5: multi-level sweep with residual.
        std::vector<ScenarioEvent>{
            {"new_order", "limit", 1, "sell", 100, 5, 1},
            {"new_order", "limit", 2, "sell", 101, 10, 2},
            {"new_order", "limit", 3, "buy", 102, 20, 3},
        },
        // Scenario 6: market on empty book rejects.
        std::vector<ScenarioEvent>{
            {"new_order", "market", 1, "buy", 0, 10, 1},
        },
        // Scenario 7: market multi-level full fill.
        std::vector<ScenarioEvent>{
            {"new_order", "limit", 1, "sell", 100, 5, 1},
            {"new_order", "limit", 2, "sell", 101, 5, 2},
            {"new_order", "market", 3, "buy", 0, 8, 3},
        },
        // Scenario 8: market partial fill, residual implicitly cancelled.
        std::vector<ScenarioEvent>{
            {"new_order", "limit", 1, "sell", 100, 5, 1},
            {"new_order", "market", 2, "buy", 0, 10, 2},
        },
        // Scenario 9: IOC fully fills.
        std::vector<ScenarioEvent>{
            {"new_order", "limit", 1, "sell", 100, 30, 1},
            {"new_order", "ioc", 2, "buy", 100, 30, 2},
        },
        // Scenario 10: IOC partial fill, residual implicitly cancelled.
        std::vector<ScenarioEvent>{
            {"new_order", "limit", 1, "sell", 100, 5, 1},
            {"new_order", "ioc", 2, "buy", 100, 20, 2},
        },
        // Scenario 11: IOC at non-crossing price acks zero filled.
        std::vector<ScenarioEvent>{
            {"new_order", "limit", 1, "sell", 105, 10, 1},
            {"new_order", "ioc", 2, "buy", 100, 10, 2},
        },
        // Scenario 12: cancel resting order.
        std::vector<ScenarioEvent>{
            {"new_order", "limit", 1, "buy", 100, 10, 1},
            {"cancel", "", 1, "buy", 0, 0, 2},
        },
        // Scenario 13: cancel unknown order rejects NotFound.
        std::vector<ScenarioEvent>{
            {"cancel", "", 99, "buy", 0, 0, 1},
        },
        // Scenario 14: cancel after full fill rejects NotFound.
        std::vector<ScenarioEvent>{
            {"new_order", "limit", 1, "sell", 100, 10, 1},
            {"new_order", "limit", 2, "buy", 100, 10, 2},
            {"cancel", "", 1, "sell", 0, 0, 3},
        },
        // Scenario 15: cancel after partial fill removes residual.
        std::vector<ScenarioEvent>{
            {"new_order", "limit", 1, "sell", 100, 10, 1},
            {"new_order", "limit", 2, "buy", 100, 4, 2},
            {"cancel", "", 1, "sell", 0, 0, 3},
        }
    ));

}  // namespace
}  // namespace meridian
