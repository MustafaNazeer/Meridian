// meridian-server: live demo binary.
//
// Three threads cooperate:
//
//   * Engine thread: a synthetic event generator (limit + market + IOC
//     mix, single symbol) feeds events into MatchingEngine::apply at
//     a configurable rate. Every accepted event publishes the new top
//     of book through the seqlock-protected snapshot on Book.
//
//   * Sampler thread: wakes at 30 Hz, reads the seqlock snapshot,
//     formats it as JSON, and hands it to the WsServer via
//     broadcast() and set_snapshot().
//
//   * Main thread: WsServer::serve_forever() runs a poll() loop on
//     the listen socket plus connected client fds. New /ws clients
//     receive the most recent snapshot immediately on connect; from
//     then on the sampler's broadcasts reach them on the next poll
//     wakeup (bounded by the 33 ms poll timeout, well within the
//     30 Hz tick budget).
//
// CLI: --port N (default 0, OS-assigned; the actual port is printed
// to stdout on the first line so the integration smoke test can pick
// it up). --rate N caps the engine event rate (default 50000 events
// per second; high enough to keep the book lively, low enough that a
// CI runner is not pinned).

#include "meridian/book.hpp"
#include "meridian/book_registry.hpp"
#include "meridian/matching.hpp"
#include "meridian/order_index.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/seqlock_snapshot.hpp"
#include "meridian/types.hpp"
#include "meridian/ws_server.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Cfg {
    int port = 0;
    std::uint64_t rate_eps = 50000;  // engine events per second
    std::uint64_t seed = 42;
};

void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "Usage: %s [--port N] [--rate N] [--seed N]\n"
                 "  --port  listen port (0 = OS-assigned, default 0)\n"
                 "  --rate  synthetic engine event rate, events/sec (default 50000)\n"
                 "  --seed  RNG seed for the synthetic event stream (default 42)\n",
                 argv0);
}

bool parse_uint(const char* s, std::uint64_t& out) {
    if (s == nullptr || *s == '\0') return false;
    char* end = nullptr;
    errno = 0;
    unsigned long long v = std::strtoull(s, &end, 10);
    if (errno != 0 || end == nullptr || *end != '\0') return false;
    out = static_cast<std::uint64_t>(v);
    return true;
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
        if (std::strcmp(a, "--port") == 0) {
            const char* v = need_value("--port");
            std::uint64_t p = 0;
            if (v == nullptr || !parse_uint(v, p) || p > 65535) {
                std::fprintf(stderr, "error: --port must be in [0, 65535]\n");
                return false;
            }
            cfg.port = static_cast<int>(p);
        } else if (std::strcmp(a, "--rate") == 0) {
            const char* v = need_value("--rate");
            if (v == nullptr || !parse_uint(v, cfg.rate_eps) || cfg.rate_eps == 0) {
                std::fprintf(stderr, "error: --rate must be a positive integer\n");
                return false;
            }
        } else if (std::strcmp(a, "--seed") == 0) {
            const char* v = need_value("--seed");
            if (v == nullptr || !parse_uint(v, cfg.seed)) {
                std::fprintf(stderr, "error: --seed must be a non-negative integer\n");
                return false;
            }
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

std::atomic<bool> g_stop{false};
meridian::ws::WsServer* g_server = nullptr;

void on_signal(int) noexcept {
    g_stop.store(true, std::memory_order_relaxed);
    if (g_server != nullptr) g_server->request_stop();
}

std::string format_snapshot(const meridian::TopOfBookSnapshot& s,
                            const char* kind) {
    std::ostringstream ss;
    ss << "{\"kind\":\"" << kind << "\","
       << "\"bid_px\":"  << (s.has_bid() ? s.best_bid_px : -1) << ","
       << "\"bid_qty\":" << s.best_bid_qty << ","
       << "\"ask_px\":"  << (s.has_ask() ? s.best_ask_px : -1) << ","
       << "\"ask_qty\":" << s.best_ask_qty << ","
       << "\"ts\":"      << s.ts
       << "}";
    return ss.str();
}

}  // namespace

int main(int argc, char** argv) {
    Cfg cfg;
    if (!parse_args(argc, argv, cfg)) return 2;

    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);
    std::signal(SIGPIPE, SIG_IGN);

    constexpr meridian::Symbol kSym = 1;
    meridian::OrderPool pool(1u << 16);
    meridian::BookRegistry registry{kSym};
    meridian::OrderIndex index;
    meridian::MatchingEngine engine(pool, registry, index);
    meridian::Book* book = registry.book(kSym);
    if (book == nullptr) {
        std::fprintf(stderr, "error: failed to acquire book for symbol %u\n", kSym);
        return 1;
    }

    meridian::ws::WsServer server(cfg.port);
    if (server.assigned_port() == 0) {
        std::fprintf(stderr, "error: WsServer failed to bind\n");
        return 1;
    }
    g_server = &server;
    // Stdout port announce: machine-parseable first line so the smoke
    // test can read it without parsing flag output.
    std::printf("port %d\n", server.assigned_port());
    std::fflush(stdout);

    // Engine thread: synthetic limit / market / cancel mix.
    std::thread engine_thr([&]() {
        std::mt19937_64 rng(cfg.seed);
        std::uniform_int_distribution<int> price_dist(95, 105);
        std::uniform_int_distribution<int> qty_dist(1, 30);
        std::uniform_int_distribution<int> bucket(0, 99);
        std::bernoulli_distribution side_dist(0.5);

        std::vector<meridian::ExecutionReport> reports;
        reports.reserve(64);
        meridian::OrderId next_id = 1;
        meridian::OrderId max_id = 0;
        meridian::Timestamp ts = 0;

        const auto period_ns = std::chrono::nanoseconds(
            cfg.rate_eps > 0 ? 1'000'000'000ULL / cfg.rate_eps : 1'000'000ULL);
        auto next_tick = std::chrono::steady_clock::now();

        while (!g_stop.load(std::memory_order_relaxed)) {
            meridian::EngineEvent ev{};
            ev.symbol = kSym;
            ev.ts = ++ts;
            ev.side = side_dist(rng) ? meridian::Side::Buy : meridian::Side::Sell;
            const int b = bucket(rng);
            if (b < 70 || max_id == 0) {
                ev.kind = meridian::EventKind::NewOrder;
                ev.type = meridian::OrderType::Limit;
                ev.order_id = next_id++;
                ev.price = price_dist(rng);
                ev.qty = qty_dist(rng);
                max_id = ev.order_id;
            } else if (b < 90) {
                ev.kind = meridian::EventKind::NewOrder;
                ev.type = meridian::OrderType::Market;
                ev.order_id = next_id++;
                ev.price = 0;
                ev.qty = qty_dist(rng) / 4 + 1;
                max_id = ev.order_id;
            } else {
                std::uniform_int_distribution<meridian::OrderId> cancel_dist(1, max_id);
                ev.kind = meridian::EventKind::Cancel;
                ev.order_id = cancel_dist(rng);
            }
            reports.clear();
            engine.apply(ev, reports);

            next_tick += period_ns;
            std::this_thread::sleep_until(next_tick);
        }
    });

    // Sampler thread: 30 Hz read of the snapshot, broadcast as a
    // JSON delta. The very first tick also publishes as the
    // "snapshot" payload that newly connecting clients receive.
    std::thread sampler_thr([&]() {
        const auto period = std::chrono::milliseconds(33);
        auto next_tick = std::chrono::steady_clock::now();
        bool first = true;
        while (!g_stop.load(std::memory_order_relaxed)) {
            const meridian::TopOfBookSnapshot snap = book->top_of_book();
            const std::string payload = format_snapshot(
                snap, first ? "snapshot" : "delta");
            server.set_snapshot(format_snapshot(snap, "snapshot"));
            server.broadcast(payload);
            first = false;
            next_tick += period;
            std::this_thread::sleep_until(next_tick);
        }
    });

    // Main thread: WS event loop. Blocks until g_stop is set or the
    // server itself decides to stop.
    server.serve_forever();

    g_stop.store(true, std::memory_order_relaxed);
    server.request_stop();
    if (engine_thr.joinable())  engine_thr.join();
    if (sampler_thr.joinable()) sampler_thr.join();
    g_server = nullptr;
    return 0;
}
