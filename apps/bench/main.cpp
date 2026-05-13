// meridian-bench: zero-I/O throughput and latency benchmark.
//
// Headline number: events per second through MatchingEngine::apply on a
// single thread, single symbol, with a deterministic synthetic event
// stream. The bench is allocation-free on the hot path: a single
// std::vector<ExecutionReport> is allocated outside the timed window
// and reused across every apply() call via the engine's out-param
// overload, and the event stream is generated up front.
//
// What gets measured:
//
//   * Throughput: total wall clock from the first measured apply() to
//     the last, divided by the measured event count.
//   * Per-event latency, captured into an HDR histogram with three
//     significant figures of precision.
//
// What is excluded from measurement:
//
//   * Event generation (pre-generated, before the timed window).
//   * Warmup events (a 10% prefix of the stream is run through
//     apply() without timing, to let the L1/L2 caches and the branch
//     predictor settle into steady state).
//   * The std::chrono::steady_clock::now() overhead itself shows up in
//     every measurement; on Linux x86_64 this is ~25 ns. It is the
//     irreducible floor of any per-event timed measurement and is
//     reported as a known cost.
//
// Output:
//
//   * Markdown summary table on stdout.
//   * JSON sidecar at --json-out (default bench/last-run.json) for
//     downstream tools (the regression check, the perf report
//     generator).

#include "meridian/book.hpp"
#include "meridian/book_registry.hpp"
#include "meridian/execution_report.hpp"
#include "meridian/matching.hpp"
#include "meridian/order_index.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <hdr/hdr_histogram.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

struct BenchConfig {
    std::uint64_t events = 1'000'000;
    std::uint64_t seed = 42;
    // Warmup events run through apply() before the timed window. A
    // small prefix lets caches and the branch predictor settle, which
    // pulls the first-event tail latency out of the steady-state
    // measurement. Fixed at 10% of total events; small enough to keep
    // the bench wall time under a couple of seconds, large enough to
    // visibly reduce p99 noise.
    std::uint64_t warmup_events = 100'000;
    std::string json_out = "bench/last-run.json";
};

void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "Usage: %s [--events N] [--seed N] [--warmup N] [--json-out PATH]\n"
                 "  --events    Number of measured events "
                 "(default: 1000000)\n"
                 "  --seed      RNG seed for deterministic generation "
                 "(default: 42)\n"
                 "  --warmup    Warmup events run before measurement "
                 "(default: 100000)\n"
                 "  --json-out  Path for JSON sidecar output "
                 "(default: bench/last-run.json)\n",
                 argv0);
}

bool parse_uint(const char* s, std::uint64_t& out) {
    if (s == nullptr || *s == '\0') {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    unsigned long long v = std::strtoull(s, &end, 10);
    if (errno != 0 || end == nullptr || *end != '\0') {
        return false;
    }
    out = static_cast<std::uint64_t>(v);
    return true;
}

bool parse_args(int argc, char** argv, BenchConfig& cfg) {
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto need_value = [&](const char* flag) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %s requires a value\n", flag);
                return nullptr;
            }
            return argv[++i];
        };
        if (std::strcmp(a, "--events") == 0) {
            const char* v = need_value("--events");
            if (v == nullptr || !parse_uint(v, cfg.events) || cfg.events == 0) {
                std::fprintf(stderr, "error: --events must be a positive integer\n");
                return false;
            }
        } else if (std::strcmp(a, "--seed") == 0) {
            const char* v = need_value("--seed");
            if (v == nullptr || !parse_uint(v, cfg.seed)) {
                std::fprintf(stderr, "error: --seed must be a non-negative integer\n");
                return false;
            }
        } else if (std::strcmp(a, "--warmup") == 0) {
            const char* v = need_value("--warmup");
            if (v == nullptr || !parse_uint(v, cfg.warmup_events)) {
                std::fprintf(stderr, "error: --warmup must be a non-negative integer\n");
                return false;
            }
        } else if (std::strcmp(a, "--json-out") == 0) {
            const char* v = need_value("--json-out");
            if (v == nullptr) {
                return false;
            }
            cfg.json_out = v;
        } else if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            std::fprintf(stderr, "error: unknown argument '%s'\n", a);
            print_usage(argv[0]);
            return false;
        }
    }
    return true;
}

// Generate a deterministic synthetic event stream. Single symbol.
// Mix per 100 events:
//
//   * 70 NewOrder Limit, alternating buy and sell, prices uniform in
//     [midpoint - 50, midpoint + 50] ticks, qty uniform in [1, 100].
//   * 20 NewOrder Market, alternating buy and sell, qty uniform in
//     [1, 20] (small so the book does not drain too fast).
//   * 10 Cancel: targets a random previously-submitted order id.
//
// Timestamps are a strictly increasing nanosecond counter. Order ids
// are monotonically increasing for new orders; cancels reuse a random
// past id (which may already be cancelled or filled - the engine
// emits Reject NotFound on those, which is fine for the bench).
//
// The mix is closer to a real trading session than the pure-limit
// stream the skeleton used: it exercises the cancel-by-id path, the
// market-against-empty-book reject path, and the matching loop's
// liquidity-consumption branch on every market order.
void generate_events(std::vector<meridian::EngineEvent>& out,
                     std::uint64_t count,
                     std::uint64_t seed) {
    constexpr meridian::Symbol kSymbol = 1;
    constexpr meridian::Price kMidpoint = 10000;
    constexpr meridian::Price kPriceJitter = 50;
    constexpr meridian::Quantity kMinLimitQty = 1;
    constexpr meridian::Quantity kMaxLimitQty = 100;
    constexpr meridian::Quantity kMinMarketQty = 1;
    constexpr meridian::Quantity kMaxMarketQty = 20;

    out.clear();
    out.reserve(count);

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> price_dist(-kPriceJitter, kPriceJitter);
    std::uniform_int_distribution<meridian::Quantity> limit_qty_dist(
        kMinLimitQty, kMaxLimitQty);
    std::uniform_int_distribution<meridian::Quantity> market_qty_dist(
        kMinMarketQty, kMaxMarketQty);
    std::uniform_int_distribution<int> bucket_dist(0, 99);

    meridian::OrderId next_id = 1;
    meridian::OrderId max_submitted_id = 0;

    for (std::uint64_t i = 0; i < count; ++i) {
        const meridian::Side side =
            (i % 2 == 0) ? meridian::Side::Buy : meridian::Side::Sell;
        const meridian::Timestamp ts = static_cast<meridian::Timestamp>(i);
        const int bucket = bucket_dist(rng);

        meridian::EngineEvent ev{};
        ev.symbol = kSymbol;
        ev.ts = ts;
        ev.side = side;

        if (bucket < 70 || max_submitted_id == 0) {
            // Limit. Always issued for the first event so cancels later
            // have a non-empty id pool to draw from.
            ev.kind = meridian::EventKind::NewOrder;
            ev.type = meridian::OrderType::Limit;
            ev.order_id = next_id++;
            ev.price = kMidpoint + price_dist(rng);
            ev.qty = limit_qty_dist(rng);
            max_submitted_id = ev.order_id;
        } else if (bucket < 90) {
            // Market.
            ev.kind = meridian::EventKind::NewOrder;
            ev.type = meridian::OrderType::Market;
            ev.order_id = next_id++;
            ev.price = 0;
            ev.qty = market_qty_dist(rng);
            max_submitted_id = ev.order_id;
        } else {
            // Cancel. Target id is uniform over previously-submitted
            // ids (1..max_submitted_id). May hit a filled or already-
            // cancelled id; the engine emits Reject NotFound, which is
            // a realistic outcome on a real order book.
            std::uniform_int_distribution<meridian::OrderId> cancel_dist(
                1, max_submitted_id);
            ev.kind = meridian::EventKind::Cancel;
            ev.order_id = cancel_dist(rng);
            ev.price = 0;
            ev.qty = 0;
        }
        out.push_back(ev);
    }
}

void write_json_sidecar(const std::string& path,
                        const BenchConfig& cfg,
                        double wall_ms,
                        double throughput_meps,
                        std::int64_t p50_ns,
                        std::int64_t p90_ns,
                        std::int64_t p99_ns,
                        std::int64_t p999_ns,
                        std::int64_t max_ns) {
    std::ofstream f(path);
    if (!f) {
        std::fprintf(stderr,
                     "warning: could not open '%s' for JSON sidecar; skipping\n",
                     path.c_str());
        return;
    }
    f << "{\n"
      << "  \"schema_version\": 2,\n"
      << "  \"events\": " << cfg.events << ",\n"
      << "  \"warmup_events\": " << cfg.warmup_events << ",\n"
      << "  \"seed\": " << cfg.seed << ",\n"
      << "  \"wall_clock_ms\": " << wall_ms << ",\n"
      << "  \"throughput_mevents_per_sec\": " << throughput_meps << ",\n"
      << "  \"latency_ns\": {\n"
      << "    \"p50\": " << p50_ns << ",\n"
      << "    \"p90\": " << p90_ns << ",\n"
      << "    \"p99\": " << p99_ns << ",\n"
      << "    \"p99_9\": " << p999_ns << ",\n"
      << "    \"max\": " << max_ns << "\n"
      << "  }\n"
      << "}\n";
}

}  // namespace

int main(int argc, char** argv) {
    BenchConfig cfg;
    if (!parse_args(argc, argv, cfg)) {
        return 2;
    }

    hdr_histogram* hist = nullptr;
    int hdr_rc = hdr_init(/*lowest_discoverable_value=*/1,
                          /*highest_trackable_value=*/1'000'000'000LL,
                          /*significant_figures=*/3,
                          &hist);
    if (hdr_rc != 0 || hist == nullptr) {
        std::fprintf(stderr, "error: hdr_init failed (rc=%d)\n", hdr_rc);
        return 1;
    }

    // Total events to generate = warmup + measured.
    const std::uint64_t total_events = cfg.warmup_events + cfg.events;

    // Pool capacity: at most one resting order per submitted id, plus
    // headroom. Real workload churn (cancels and full fills release
    // slots) keeps actual peak utilisation well below this bound;
    // dimensioned for the worst case where every event is a passive
    // limit that rests forever.
    const std::size_t pool_capacity = static_cast<std::size_t>(total_events) + 1024;
    meridian::OrderPool pool(pool_capacity);
    meridian::BookRegistry registry{1};
    meridian::OrderIndex index;
    // Bench mode: disable the v1.1 observability (depth/trade publish,
    // latency histogram) so the measurement reflects pure matching
    // throughput rather than the live-demo data path.
    meridian::MatchingEngine engine(pool, registry, index, /*observability=*/false);

    std::vector<meridian::EngineEvent> events;
    generate_events(events, total_events, cfg.seed);

    // Single reusable report buffer. Reserve generously so apply()
    // never grows it during the timed window. The hot path is
    // allocation-free under the new apply() out-param overload.
    std::vector<meridian::ExecutionReport> reports;
    reports.reserve(1024);

    using Clock = std::chrono::steady_clock;
    using ns = std::chrono::nanoseconds;

    // Warmup pass: drive events through apply() without timing.
    for (std::uint64_t i = 0; i < cfg.warmup_events; ++i) {
        reports.clear();
        engine.apply(events[i], reports);
    }

    // Timed pass.
    const auto wall_start = Clock::now();
    for (std::uint64_t i = cfg.warmup_events; i < total_events; ++i) {
        reports.clear();
        const auto t0 = Clock::now();
        engine.apply(events[i], reports);
        const auto t1 = Clock::now();
        const std::int64_t dt =
            std::chrono::duration_cast<ns>(t1 - t0).count();
        hdr_record_value(hist, dt > 0 ? dt : 1);
    }
    const auto wall_end = Clock::now();

    const std::int64_t wall_ns =
        std::chrono::duration_cast<ns>(wall_end - wall_start).count();
    const double wall_ms = static_cast<double>(wall_ns) / 1.0e6;
    const double wall_s = static_cast<double>(wall_ns) / 1.0e9;
    const double throughput_eps =
        wall_s > 0.0 ? static_cast<double>(cfg.events) / wall_s : 0.0;
    const double throughput_meps = throughput_eps / 1.0e6;

    const std::int64_t p50  = hdr_value_at_percentile(hist, 50.0);
    const std::int64_t p90  = hdr_value_at_percentile(hist, 90.0);
    const std::int64_t p99  = hdr_value_at_percentile(hist, 99.0);
    const std::int64_t p999 = hdr_value_at_percentile(hist, 99.9);
    const std::int64_t hmax = hdr_max(hist);

    std::printf("\n");
    std::printf("| metric | value |\n");
    std::printf("|---|---|\n");
    std::printf("| events processed (measured) | %llu |\n",
                static_cast<unsigned long long>(cfg.events));
    std::printf("| warmup events (excluded) | %llu |\n",
                static_cast<unsigned long long>(cfg.warmup_events));
    std::printf("| wall clock | %.1f ms |\n", wall_ms);
    std::printf("| throughput | %.2f M evt/s |\n", throughput_meps);
    std::printf("| p50 latency | %lld ns |\n", static_cast<long long>(p50));
    std::printf("| p90 latency | %lld ns |\n", static_cast<long long>(p90));
    std::printf("| p99 latency | %lld ns |\n", static_cast<long long>(p99));
    std::printf("| p99.9 latency | %lld ns |\n", static_cast<long long>(p999));
    std::printf("| max latency | %lld ns |\n", static_cast<long long>(hmax));
    std::printf("\n");

    write_json_sidecar(cfg.json_out, cfg, wall_ms, throughput_meps,
                       p50, p90, p99, p999, hmax);

    hdr_close(hist);
    return 0;
}
