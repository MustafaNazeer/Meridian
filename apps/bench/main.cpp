// meridian-bench: Phase 1 skeleton.
//
// This binary is a working harness, not a regression gate. It generates a
// deterministic stream of synthetic limit orders, drives them through the
// matching engine, captures per-event latency in an HDRHistogram, and prints
// a Markdown summary table. The throughput and latency numbers it reports in
// Phase 1 are informational only; the headline 6M events/sec target is
// enforced in Phase 5 against bench/baseline.json.
//
// The pre-generated event vector and the timed apply loop are deliberately
// kept separate so generation cost does not contaminate the measurement.

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
    std::string json_out = "bench/last-run.json";
};

void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "Usage: %s [--events N] [--seed N] [--json-out PATH]\n"
                 "  --events    Number of synthetic events to generate "
                 "(default: 1000000)\n"
                 "  --seed      RNG seed for deterministic generation "
                 "(default: 42)\n"
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

// Generate a deterministic stream of synthetic limit-order NewOrder events.
// Sides alternate Buy/Sell. Prices are drawn uniformly from
// [midpoint - 50, midpoint + 50] ticks. Quantities are drawn uniformly from
// [1, 100]. Timestamps are a strictly increasing nanosecond counter.
void generate_events(std::vector<meridian::EngineEvent>& out,
                     std::uint64_t count,
                     std::uint64_t seed) {
    constexpr meridian::Symbol kSymbol = 1;
    constexpr meridian::Price kMidpoint = 10000;
    constexpr meridian::Price kPriceJitter = 50;
    constexpr meridian::Quantity kMinQty = 1;
    constexpr meridian::Quantity kMaxQty = 100;

    out.clear();
    out.reserve(count);

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> price_dist(-kPriceJitter, kPriceJitter);
    std::uniform_int_distribution<meridian::Quantity> qty_dist(kMinQty, kMaxQty);

    for (std::uint64_t i = 0; i < count; ++i) {
        meridian::Side side =
            (i % 2 == 0) ? meridian::Side::Buy : meridian::Side::Sell;
        meridian::Price price = kMidpoint + price_dist(rng);
        meridian::Quantity qty = qty_dist(rng);
        meridian::OrderId order_id = static_cast<meridian::OrderId>(i + 1);
        meridian::Timestamp ts = static_cast<meridian::Timestamp>(i);

        out.push_back(meridian::EngineEvent{
            .kind = meridian::EventKind::NewOrder,
            .symbol = kSymbol,
            .ts = ts,
            .order_id = order_id,
            .side = side,
            .type = meridian::OrderType::Limit,
            .price = price,
            .qty = qty,
        });
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
      << "  \"phase\": 1,\n"
      << "  \"informational\": true,\n"
      << "  \"events\": " << cfg.events << ",\n"
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

    // HDRHistogram: track latencies from 1 ns to 1 second with 3 significant
    // figures. 1 second is far above any plausible per-event latency; the
    // upper bound only affects bucket allocation, not measurement accuracy.
    hdr_histogram* hist = nullptr;
    int hdr_rc = hdr_init(/*lowest_discoverable_value=*/1,
                          /*highest_trackable_value=*/1'000'000'000LL,
                          /*significant_figures=*/3,
                          &hist);
    if (hdr_rc != 0 || hist == nullptr) {
        std::fprintf(stderr, "error: hdr_init failed (rc=%d)\n", hdr_rc);
        return 1;
    }

    // Pool capacity = events + 1024 headroom. Generation produces only
    // NewOrder events (no cancels in Phase 1 bench), so no slot is ever
    // released; one slot per event plus headroom is enough.
    const std::size_t pool_capacity = static_cast<std::size_t>(cfg.events) + 1024;
    meridian::OrderPool pool(pool_capacity);
    meridian::BookRegistry registry{1};
    meridian::OrderIndex index;
    meridian::MatchingEngine engine(pool, registry, index);

    std::vector<meridian::EngineEvent> events;
    generate_events(events, cfg.events, cfg.seed);

    using Clock = std::chrono::steady_clock;
    using ns = std::chrono::nanoseconds;

    const auto wall_start = Clock::now();
    for (const auto& ev : events) {
        const auto t0 = Clock::now();
        // Discard the returned reports; the bench measures apply latency, not
        // downstream report consumption. The vector allocation inside apply()
        // is a known Phase 1 cost the Engine Developer documents in the
        // matching-loop allocations note; Phase 5 retires it.
        (void)engine.apply(ev);
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
    std::printf("| events processed | %llu |\n",
                static_cast<unsigned long long>(cfg.events));
    std::printf("| wall clock | %.1f ms |\n", wall_ms);
    std::printf("| throughput | %.1f M evt/s |\n", throughput_meps);
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
