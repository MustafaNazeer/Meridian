// meridian-replay: read an ITCH 5.0 tape, dispatch through the matching
// engine, write JSON Lines audit output.
//
// Scope (this milestone): single symbol, A and D message types only.
// The integration test (tests/integration/test_itch_replay.cpp) drives
// this binary alongside the Python reference (tests/reference/itch_replay.py)
// and byte-diffs their JSONL outputs.
//
// Stock symbol mapping: today the parser hands us the raw 8-char ITCH
// stock string. The single-symbol scope means we map every unique
// stock string to symbol id 1 the first time we see it. Multi-symbol
// dispatch (one symbol id per distinct ITCH stock) lands when the
// integration corpus needs it.

#include "meridian/book.hpp"
#include "meridian/book_registry.hpp"
#include "meridian/execution_report.hpp"
#include "meridian/itch.hpp"
#include "meridian/matching.hpp"
#include "meridian/order_index.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Cfg {
    std::string tape;
    std::string audit;
};

void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "Usage: %s --tape PATH --audit PATH\n"
                 "  --tape   ITCH 5.0 binary tape (required)\n"
                 "  --audit  JSON Lines audit output path (required)\n",
                 argv0);
}

bool parse_args(int argc, char** argv, Cfg& cfg) {
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto need_value = [&](const char* flag) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %s requires a value\n", flag);
                return nullptr;
            }
            return argv[++i];
        };
        if (std::strcmp(a, "--tape") == 0) {
            const char* v = need_value("--tape");
            if (v == nullptr) return false;
            cfg.tape = v;
        } else if (std::strcmp(a, "--audit") == 0) {
            const char* v = need_value("--audit");
            if (v == nullptr) return false;
            cfg.audit = v;
        } else if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            std::fprintf(stderr, "error: unknown argument '%s'\n", a);
            print_usage(argv[0]);
            return false;
        }
    }
    if (cfg.tape.empty() || cfg.audit.empty()) {
        std::fprintf(stderr, "error: --tape and --audit are required\n");
        print_usage(argv[0]);
        return false;
    }
    return true;
}

bool read_file(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "error: could not open '%s' for read\n", path.c_str());
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(f),
               std::istreambuf_iterator<char>());
    return true;
}

void write_jsonl(std::ofstream& out, const meridian::ExecutionReport& r) {
    const char* kind = "ack";
    switch (r.kind) {
        case meridian::ReportKind::Acknowledge: kind = "ack"; break;
        case meridian::ReportKind::Fill:        kind = "fill"; break;
        case meridian::ReportKind::Cancel:      kind = "cancel"; break;
        case meridian::ReportKind::Reject:      kind = "reject"; break;
    }
    const char* side = (r.side == meridian::Side::Buy) ? "buy" : "sell";
    const char* reason = "none";
    switch (r.reject_reason) {
        case meridian::RejectReason::None:                  reason = "none"; break;
        case meridian::RejectReason::EmptyBook:             reason = "empty_book"; break;
        case meridian::RejectReason::NotFound:              reason = "not_found"; break;
        case meridian::RejectReason::InsufficientLiquidity: reason = "insufficient_liquidity"; break;
        case meridian::RejectReason::WouldCross:            reason = "would_cross"; break;
        case meridian::RejectReason::UnknownSymbol:         reason = "unknown_symbol"; break;
    }
    // Alphabetical key order so the JSON Lines output diffs cleanly
    // against the Python reference (which uses json.dumps(sort_keys=True)).
    out << "{"
        << "\"kind\":\"" << kind << "\","
        << "\"order_id\":" << r.order_id << ","
        << "\"price\":" << r.price << ","
        << "\"qty\":" << r.qty << ","
        << "\"reject_reason\":\"" << reason << "\","
        << "\"side\":\"" << side << "\","
        << "\"ts\":" << r.ts
        << "}\n";
}

}  // namespace

int main(int argc, char** argv) {
    Cfg cfg;
    if (!parse_args(argc, argv, cfg)) return 2;

    std::vector<std::uint8_t> tape;
    if (!read_file(cfg.tape, tape)) return 1;

    std::ofstream audit(cfg.audit, std::ios::binary);
    if (!audit) {
        std::fprintf(stderr, "error: could not open '%s' for write\n", cfg.audit.c_str());
        return 1;
    }

    // Single-symbol mapping for this milestone: every distinct ITCH
    // stock id encountered maps to symbol id 1. Multi-symbol fan-out
    // lands when the integration corpus requires it.
    constexpr meridian::Symbol kSym = 1;

    // Pool capacity: bounded by the worst case of "every Add rests
    // forever". For typical workloads this is far too generous; we
    // err on the side of safety since the bench measures throughput,
    // not memory footprint.
    const std::size_t pool_capacity = tape.size() / 10 + 1024;
    meridian::OrderPool pool(pool_capacity);
    meridian::BookRegistry registry{kSym};
    meridian::OrderIndex index;
    meridian::MatchingEngine engine(pool, registry, index);

    std::vector<meridian::ExecutionReport> reports;
    reports.reserve(64);

    meridian::itch::Parser parser(tape.data(), tape.size());
    meridian::itch::ParsedMessage msg{};
    std::uint64_t accepted = 0;
    std::uint64_t skipped = 0;
    while (true) {
        const auto rc = parser.next(msg);
        if (rc == meridian::itch::ParseStatus::EndOfStream) break;
        if (rc != meridian::itch::ParseStatus::Ok) {
            std::fprintf(stderr,
                         "error: parser returned status %d at offset %zu\n",
                         static_cast<int>(rc), parser.bytes_consumed());
            return 1;
        }
        meridian::EngineEvent ev{};
        ev.symbol = kSym;
        if (msg.kind == meridian::itch::MessageKind::AddOrder) {
            ev.kind = meridian::EventKind::NewOrder;
            ev.type = meridian::OrderType::Limit;
            ev.ts = msg.add.ts_ns;
            ev.order_id = msg.add.order_ref;
            ev.side = msg.add.side;
            ev.price = msg.add.price;
            ev.qty = msg.add.shares;
        } else if (msg.kind == meridian::itch::MessageKind::OrderDelete) {
            ev.kind = meridian::EventKind::Cancel;
            ev.ts = msg.del.ts_ns;
            ev.order_id = msg.del.order_ref;
        } else {
            ++skipped;
            continue;
        }
        reports.clear();
        engine.apply(ev, reports);
        for (const auto& r : reports) write_jsonl(audit, r);
        ++accepted;
    }

    std::fprintf(stderr,
                 "replay: %llu messages dispatched, %llu skipped (system events / unknown types)\n",
                 static_cast<unsigned long long>(accepted),
                 static_cast<unsigned long long>(skipped));
    return 0;
}
