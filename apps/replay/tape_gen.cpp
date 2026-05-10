// meridian-tape-gen: deterministic ITCH 5.0 binary tape generator.
//
// Emits a synthetic tape that the integration test and ad-hoc bench
// runs can consume. The tape format is bit-for-bit ITCH 5.0 (2-byte
// big-endian length prefix per message, body fields per the NASDAQ
// spec) so a tape produced here parses cleanly through any standard
// ITCH 5.0 reader, including meridian-replay and the Python reference
// at tests/reference/itch_replay.py.
//
// Scope: only Add Order ('A') and Order Delete ('D') messages, plus a
// 'S' System Event marker at start and end. This is the same subset
// the parser supports today; messages we do not parse yet (X, U, F,
// E, C) are not emitted.
//
// Usage: meridian-tape-gen --out PATH [--messages N] [--seed N] [--cancel-pct N]
//
// Defaults: 100000 messages, seed=42, 30% delete (the rest are adds).

#include "meridian/itch.hpp"
#include "meridian/types.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

struct Cfg {
    std::string out;
    std::uint64_t messages = 100000;
    std::uint64_t seed = 42;
    int cancel_pct = 30;
};

void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "Usage: %s --out PATH [--messages N] [--seed N] [--cancel-pct N]\n"
                 "  --out         output tape path (required)\n"
                 "  --messages    total messages to emit (default 100000)\n"
                 "  --seed        RNG seed (default 42)\n"
                 "  --cancel-pct  percent of messages that are Delete (default 30)\n",
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
        if (std::strcmp(a, "--out") == 0) {
            const char* v = need_value("--out");
            if (v == nullptr) return false;
            cfg.out = v;
        } else if (std::strcmp(a, "--messages") == 0) {
            const char* v = need_value("--messages");
            if (v == nullptr || !parse_uint(v, cfg.messages) || cfg.messages == 0) {
                std::fprintf(stderr, "error: --messages must be a positive integer\n");
                return false;
            }
        } else if (std::strcmp(a, "--seed") == 0) {
            const char* v = need_value("--seed");
            if (v == nullptr || !parse_uint(v, cfg.seed)) {
                std::fprintf(stderr, "error: --seed must be a non-negative integer\n");
                return false;
            }
        } else if (std::strcmp(a, "--cancel-pct") == 0) {
            const char* v = need_value("--cancel-pct");
            std::uint64_t n = 0;
            if (v == nullptr || !parse_uint(v, n) || n > 99) {
                std::fprintf(stderr, "error: --cancel-pct must be in [0, 99]\n");
                return false;
            }
            cfg.cancel_pct = static_cast<int>(n);
        } else if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            std::fprintf(stderr, "error: unknown argument '%s'\n", a);
            print_usage(argv[0]);
            return false;
        }
    }
    if (cfg.out.empty()) {
        std::fprintf(stderr, "error: --out is required\n");
        print_usage(argv[0]);
        return false;
    }
    return true;
}

void put_be16(std::vector<std::uint8_t>& buf, std::uint16_t v) {
    buf.push_back(static_cast<std::uint8_t>(v >> 8));
    buf.push_back(static_cast<std::uint8_t>(v & 0xFFu));
}
void put_be32(std::vector<std::uint8_t>& buf, std::uint32_t v) {
    buf.push_back(static_cast<std::uint8_t>(v >> 24));
    buf.push_back(static_cast<std::uint8_t>(v >> 16));
    buf.push_back(static_cast<std::uint8_t>(v >> 8));
    buf.push_back(static_cast<std::uint8_t>(v & 0xFFu));
}
void put_be48(std::vector<std::uint8_t>& buf, std::uint64_t v) {
    buf.push_back(static_cast<std::uint8_t>((v >> 40) & 0xFFu));
    buf.push_back(static_cast<std::uint8_t>((v >> 32) & 0xFFu));
    buf.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
    buf.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
    buf.push_back(static_cast<std::uint8_t>((v >> 8)  & 0xFFu));
    buf.push_back(static_cast<std::uint8_t>(v & 0xFFu));
}
void put_be64(std::vector<std::uint8_t>& buf, std::uint64_t v) {
    buf.push_back(static_cast<std::uint8_t>((v >> 56) & 0xFFu));
    buf.push_back(static_cast<std::uint8_t>((v >> 48) & 0xFFu));
    buf.push_back(static_cast<std::uint8_t>((v >> 40) & 0xFFu));
    buf.push_back(static_cast<std::uint8_t>((v >> 32) & 0xFFu));
    buf.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
    buf.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
    buf.push_back(static_cast<std::uint8_t>((v >> 8)  & 0xFFu));
    buf.push_back(static_cast<std::uint8_t>(v & 0xFFu));
}

void emit_system_event(std::vector<std::uint8_t>& buf,
                       std::uint64_t ts_ns,
                       char event_code) {
    put_be16(buf, meridian::itch::kSystemEventSize);
    buf.push_back('S');
    put_be16(buf, 0);  // stock locate
    put_be16(buf, 0);  // tracking number
    put_be48(buf, ts_ns);
    buf.push_back(static_cast<std::uint8_t>(event_code));
}

void emit_add(std::vector<std::uint8_t>& buf,
              std::uint64_t ts_ns,
              std::uint64_t order_ref,
              char side,
              std::uint32_t shares,
              const char* stock8,
              std::uint32_t price) {
    put_be16(buf, meridian::itch::kAddOrderSize);
    buf.push_back('A');
    put_be16(buf, 0);
    put_be16(buf, 0);
    put_be48(buf, ts_ns);
    put_be64(buf, order_ref);
    buf.push_back(static_cast<std::uint8_t>(side));
    put_be32(buf, shares);
    for (int i = 0; i < 8; ++i) {
        buf.push_back(static_cast<std::uint8_t>(stock8[i]));
    }
    put_be32(buf, price);
}

void emit_delete(std::vector<std::uint8_t>& buf,
                 std::uint64_t ts_ns,
                 std::uint64_t order_ref) {
    put_be16(buf, meridian::itch::kOrderDeleteSize);
    buf.push_back('D');
    put_be16(buf, 0);
    put_be16(buf, 0);
    put_be48(buf, ts_ns);
    put_be64(buf, order_ref);
}

}  // namespace

int main(int argc, char** argv) {
    Cfg cfg;
    if (!parse_args(argc, argv, cfg)) return 2;

    std::vector<std::uint8_t> tape;
    // Heuristic preallocation: average message length around 30 bytes plus 2-byte length prefix.
    tape.reserve(cfg.messages * 38 + 32);

    std::mt19937_64 rng(cfg.seed);
    std::uniform_int_distribution<int> price_dist(-50, 50);
    std::uniform_int_distribution<std::uint32_t> shares_dist(1, 100);
    std::uniform_int_distribution<int> bucket_dist(0, 99);
    constexpr std::uint32_t kMidpoint = 1'000'000;  // $100.0000 in 1/10000 units
    constexpr char kStock[8] = {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '};

    std::uint64_t ts_ns = 1'000'000'000ULL;  // start at 1 s past midnight
    emit_system_event(tape, ts_ns, 'O');  // start of messages

    std::uint64_t next_ref = 1;
    std::uint64_t max_ref = 0;

    for (std::uint64_t i = 0; i < cfg.messages; ++i) {
        ts_ns += 100;  // 100 ns between messages
        const int b = bucket_dist(rng);
        if (b < cfg.cancel_pct && max_ref > 0) {
            std::uniform_int_distribution<std::uint64_t> ref_dist(1, max_ref);
            const std::uint64_t target = ref_dist(rng);
            emit_delete(tape, ts_ns, target);
        } else {
            const char side = ((i & 1u) == 0u) ? 'B' : 'S';
            const std::uint32_t shares = shares_dist(rng);
            const std::uint32_t price =
                static_cast<std::uint32_t>(static_cast<int>(kMidpoint) + price_dist(rng));
            emit_add(tape, ts_ns, next_ref, side, shares, kStock, price);
            max_ref = next_ref;
            ++next_ref;
        }
    }

    ts_ns += 100;
    emit_system_event(tape, ts_ns, 'C');  // end of messages

    std::ofstream f(cfg.out, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "error: could not open '%s' for write\n", cfg.out.c_str());
        return 1;
    }
    f.write(reinterpret_cast<const char*>(tape.data()),
            static_cast<std::streamsize>(tape.size()));
    if (!f) {
        std::fprintf(stderr, "error: short write to '%s'\n", cfg.out.c_str());
        return 1;
    }
    std::fprintf(stderr, "wrote %zu bytes (%llu messages plus 2 system events) to %s\n",
                 tape.size(),
                 static_cast<unsigned long long>(cfg.messages),
                 cfg.out.c_str());
    return 0;
}
