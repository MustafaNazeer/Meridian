// Differential property test: random EngineEvent sequences run through
// both the C++ MatchingEngine and the Python reference under
// tests/reference/, then diffed byte for byte on the JSON Lines wire
// format. This is the strongest invariant in the property suite: it
// asserts that the engine matches the reference on every report, not
// just that an internal predicate holds.
//
// Each case spawns a Python subprocess, so this suite uses fewer cases
// and shorter sequences than the in-process invariant suite. CI wall
// time stays under ~30s on the GitHub Actions ubuntu-24.04 runner.

#include "harness.hpp"
#include "meridian/types.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace meridian::property {
namespace {

constexpr int kCases = 100;
constexpr std::size_t kEventsPerCase = 30;
constexpr std::uint64_t kSeedBase = 0xD1FF1A40u;

std::string event_to_jsonl(const EngineEvent& e) {
    std::ostringstream ss;
    if (e.kind == EventKind::Cancel) {
        ss << "{"
           << "\"kind\":\"cancel\","
           << "\"order_id\":" << e.order_id << ","
           << "\"ts\":" << e.ts
           << "}";
    } else {
        const char* type = "limit";
        if (e.type == OrderType::Market)        type = "market";
        else if (e.type == OrderType::IOC)      type = "ioc";
        else if (e.type == OrderType::PostOnly) type = "postonly";
        else if (e.type == OrderType::FOK)      type = "fok";
        const char* side = (e.side == Side::Buy) ? "buy" : "sell";
        ss << "{"
           << "\"kind\":\"new_order\","
           << "\"order_id\":" << e.order_id << ","
           << "\"price\":" << e.price << ","
           << "\"qty\":" << e.qty << ","
           << "\"side\":\"" << side << "\","
           << "\"symbol\":" << e.symbol << ","
           << "\"ts\":" << e.ts << ","
           << "\"type\":\"" << type << "\""
           << "}";
    }
    return ss.str();
}

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
        case RejectReason::UnknownSymbol:         reason = "unknown_symbol"; break;
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

std::vector<std::string> run_python_reference(
    const std::vector<GeneratedEvent>& events, const std::string& workdir) {
    const std::string input_path  = workdir + "/input.jsonl";
    const std::string output_path = workdir + "/output.jsonl";
    {
        std::ofstream f(input_path);
        for (const auto& g : events) f << event_to_jsonl(g.ev) << "\n";
    }
    const std::string cmd = "PYTHONPATH=" SOURCE_REFERENCE_DIR
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

TEST(Differential, RandomSequencesAgreeWithReference) {
    GenConfig cfg;
    cfg.length = kEventsPerCase;
    for (int i = 0; i < kCases; ++i) {
        const std::uint64_t seed = kSeedBase + static_cast<std::uint64_t>(i);
        SCOPED_TRACE(::testing::Message() << "seed=" << seed);
        auto events = EventGenerator(seed, cfg).generate();
        auto run = run_engine(events);

        char workdir[] = "/tmp/meridian_pbt_diff_XXXXXX";
        ASSERT_NE(mkdtemp(workdir), nullptr);
        auto py_lines = run_python_reference(events, workdir);
        const std::string rm_cmd = "rm -rf " + std::string(workdir);
        [[maybe_unused]] int rm_rc = std::system(rm_cmd.c_str());

        std::vector<std::string> cpp_lines;
        cpp_lines.reserve(run.reports.size());
        for (const auto& r : run.reports) cpp_lines.push_back(report_to_jsonl(r));

        ASSERT_EQ(cpp_lines.size(), py_lines.size())
            << "C++ produced " << cpp_lines.size()
            << " reports, Python produced " << py_lines.size();
        for (std::size_t k = 0; k < cpp_lines.size(); ++k) {
            ASSERT_EQ(cpp_lines[k], py_lines[k])
                << "report " << k << " disagrees";
        }
    }
}

}  // namespace
}  // namespace meridian::property
