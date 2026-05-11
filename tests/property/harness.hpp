#pragma once

// Shared property-test harness: deterministic event-sequence generator,
// per-OrderId shadow-state bookkeeping, and a thin wrapper around
// MatchingEngine::apply that returns the produced report stream alongside
// the shadow state at the end of the sequence.
//
// The generator is seeded by std::mt19937, so a failing case is fully
// reproducible from its seed alone. Each invariant test instantiates the
// generator with a known set of seeds (kSeedBase + i for i in [0, kCases))
// and reports the failing seed in the assertion message so a regression
// can be reproduced by running a single seed in isolation.
//
// The shadow tracker rebuilds, from the wire-format report stream alone,
// what the engine should have done with each submitted OrderId: how much
// was filled, how much was cancelled, whether it ended up resting. The
// invariants are then checks against this reconstruction; they do not
// peek at engine internals beyond what the report stream exposes (which
// is the same view a real downstream consumer would have).

#include "meridian/book.hpp"
#include "meridian/book_registry.hpp"
#include "meridian/execution_report.hpp"
#include "meridian/matching.hpp"
#include "meridian/order_index.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace meridian::property {

inline constexpr std::array<Symbol, 5> kDemoSymbols{1, 2, 3, 4, 5};

// One generated event plus enough metadata for the shadow tracker to
// classify the outcome. The fields mirror EngineEvent so the test can
// hand them straight to MatchingEngine::apply.
struct GeneratedEvent {
    EngineEvent ev;
};

// Per-event generator knobs. Defaults are tuned for the invariant suite:
// short bursts of mixed limit / market / IOC / post-only / FOK traffic
// across the demo symbol set, with a non-trivial cancel ratio so the
// cancel-by-id path is exercised on every seed.
struct GenConfig {
    std::size_t length = 100;
    Price price_min = 50;
    Price price_max = 150;
    Quantity qty_min = 1;
    Quantity qty_max = 30;
    // Probability buckets (must sum to 1.0).
    double p_new_order = 0.70;
    double p_cancel    = 0.30;
    // Conditional on NewOrder. The buckets sum to 1.0 across the five
    // implemented order types.
    double p_limit     = 0.50;
    double p_market    = 0.15;
    double p_ioc       = 0.15;
    double p_post_only = 0.10;
    double p_fok       = 0.10;
    // Cancels: with this probability, the cancel targets an OrderId that
    // was never submitted (exercising the NotFound branch). Otherwise it
    // picks uniformly from previously-submitted ids.
    double p_cancel_unknown = 0.20;
};

class EventGenerator {
public:
    EventGenerator(std::uint64_t seed, GenConfig cfg = {}) noexcept
        : rng_(seed), cfg_(cfg) {}

    std::vector<GeneratedEvent> generate() {
        std::vector<GeneratedEvent> out;
        out.reserve(cfg_.length);
        std::vector<OrderId> submitted_ids;
        submitted_ids.reserve(cfg_.length);

        std::uniform_real_distribution<double> uni01(0.0, 1.0);
        std::uniform_int_distribution<Price> price_dist(cfg_.price_min, cfg_.price_max);
        std::uniform_int_distribution<Quantity> qty_dist(cfg_.qty_min, cfg_.qty_max);
        std::uniform_int_distribution<std::size_t> sym_dist(0, kDemoSymbols.size() - 1);
        std::bernoulli_distribution side_dist(0.5);

        OrderId next_id = 1;
        Timestamp ts = 0;
        for (std::size_t i = 0; i < cfg_.length; ++i) {
            ++ts;
            const double r = uni01(rng_);
            if (r < cfg_.p_new_order || submitted_ids.empty()) {
                // NewOrder: pick a type, side, symbol, price, qty.
                EngineEvent ev{};
                ev.kind = EventKind::NewOrder;
                ev.symbol = kDemoSymbols[sym_dist(rng_)];
                ev.side = side_dist(rng_) ? Side::Buy : Side::Sell;
                ev.order_id = next_id++;
                ev.ts = ts;
                ev.price = price_dist(rng_);
                ev.qty = qty_dist(rng_);
                const double t = uni01(rng_);
                const double cum_limit     = cfg_.p_limit;
                const double cum_market    = cum_limit + cfg_.p_market;
                const double cum_ioc       = cum_market + cfg_.p_ioc;
                const double cum_post_only = cum_ioc + cfg_.p_post_only;
                if (t < cum_limit) {
                    ev.type = OrderType::Limit;
                } else if (t < cum_market) {
                    ev.type = OrderType::Market;
                    ev.price = 0;
                } else if (t < cum_ioc) {
                    ev.type = OrderType::IOC;
                } else if (t < cum_post_only) {
                    ev.type = OrderType::PostOnly;
                } else {
                    ev.type = OrderType::FOK;
                }
                submitted_ids.push_back(ev.order_id);
                out.push_back({ev});
            } else {
                // Cancel: hit a known id most of the time, an unknown id
                // otherwise. Either way, the engine must produce a report
                // (Cancel for a live order, Reject NotFound for the rest).
                EngineEvent ev{};
                ev.kind = EventKind::Cancel;
                ev.ts = ts;
                if (uni01(rng_) < cfg_.p_cancel_unknown) {
                    ev.order_id = next_id + 100000;  // guaranteed unseen
                } else {
                    std::uniform_int_distribution<std::size_t> pick(
                        0, submitted_ids.size() - 1);
                    ev.order_id = submitted_ids[pick(rng_)];
                }
                // Side/price/qty/symbol on cancel are unread by the
                // engine but populated for completeness.
                ev.side = Side::Buy;
                ev.symbol = kDemoSymbols[0];
                ev.price = 0;
                ev.qty = 0;
                out.push_back({ev});
            }
        }
        return out;
    }

private:
    std::mt19937_64 rng_;
    GenConfig cfg_;
};

// Per-OrderId shadow record reconstructed from the report stream.
struct ShadowOrder {
    bool submitted = false;
    OrderType type{};
    Side side{};
    Symbol symbol{};
    Price price = 0;
    Quantity qty_submitted = 0;
    Timestamp ts_arrival = 0;
    // Aggregates updated as reports flow in.
    bool acked = false;
    bool rejected_at_submission = false;  // single-Reject path (market on empty book, or unknown symbol)
    Quantity qty_filled = 0;
    Quantity qty_cancelled = 0;
    bool cancelled = false;
    bool currently_resting = false;
};

class ShadowTracker {
public:
    void on_event(const EngineEvent& ev) {
        if (ev.kind == EventKind::NewOrder) {
            ShadowOrder& s = orders_[ev.order_id];
            s.submitted = true;
            s.type = ev.type;
            s.side = ev.side;
            s.symbol = ev.symbol;
            s.price = ev.price;
            s.qty_submitted = ev.qty;
            s.ts_arrival = ev.ts;
            submitted_in_order_.push_back(ev.order_id);
        }
    }

    void on_report(const ExecutionReport& r) {
        switch (r.kind) {
            case ReportKind::Acknowledge: {
                ShadowOrder& s = orders_[r.order_id];
                s.acked = true;
                break;
            }
            case ReportKind::Fill: {
                ShadowOrder& s = orders_[r.order_id];
                s.qty_filled += r.qty;
                break;
            }
            case ReportKind::Cancel: {
                ShadowOrder& s = orders_[r.order_id];
                s.cancelled = true;
                s.qty_cancelled = r.qty;
                s.currently_resting = false;
                break;
            }
            case ReportKind::Reject: {
                if (r.reject_reason == RejectReason::EmptyBook ||
                    r.reject_reason == RejectReason::UnknownSymbol ||
                    r.reject_reason == RejectReason::WouldCross ||
                    r.reject_reason == RejectReason::InsufficientLiquidity) {
                    ShadowOrder& s = orders_[r.order_id];
                    s.rejected_at_submission = true;
                }
                // NotFound from a cancel is an outcome of the cancel
                // event and does not change any shadow order state.
                break;
            }
        }
    }

    // Mark which shadow orders ended up resting on the book at the end
    // of the sequence. A submitted order is "resting" iff it was acked,
    // never rejected, never cancelled, and its filled quantity is less
    // than its submitted quantity, and it is a Limit (markets and IOCs
    // never rest).
    void finalize() {
        for (auto& [id, s] : orders_) {
            if (!s.submitted || s.rejected_at_submission || s.cancelled) {
                s.currently_resting = false;
                continue;
            }
            if (s.qty_filled >= s.qty_submitted) {
                s.currently_resting = false;
                continue;
            }
            // Limit and PostOnly can rest. Market, IOC, and FOK never
            // rest; for Market and IOC the residual is implicitly
            // cancelled (no Cancel report) and the shadow accounts for
            // that as "implicit_cancel" on demand in the conservation
            // law check below. FOK only ever fills in full or rejects;
            // a non-rejected FOK has qty_filled == qty_submitted, so
            // it never reaches this branch.
            s.currently_resting = (s.type == OrderType::Limit ||
                                   s.type == OrderType::PostOnly);
        }
    }

    [[nodiscard]] const std::unordered_map<OrderId, ShadowOrder>& orders() const noexcept {
        return orders_;
    }

    [[nodiscard]] const std::vector<OrderId>& submitted_in_order() const noexcept {
        return submitted_in_order_;
    }

    // The implicit-cancel quantity for a market or IOC that walked away
    // with unfilled residual: submitted - filled. Limit and PostOnly
    // rest their residual on the book (no implicit cancel); FOK either
    // fills in full or is rejected upfront (no residual to cancel).
    [[nodiscard]] Quantity implicit_cancel_qty(const ShadowOrder& s) const noexcept {
        if (!s.submitted || s.rejected_at_submission || s.cancelled) return 0;
        if (s.type == OrderType::Limit || s.type == OrderType::PostOnly) return 0;
        if (s.type == OrderType::FOK) return 0;
        return s.qty_submitted - s.qty_filled;
    }

    // Resting quantity for a limit order that ended the sequence on the
    // book: submitted - filled. For everything else this returns 0.
    [[nodiscard]] Quantity resting_qty(const ShadowOrder& s) const noexcept {
        if (!s.currently_resting) return 0;
        return s.qty_submitted - s.qty_filled;
    }

    // Sum of resting quantity on `sym` for orders on the given `side`,
    // computed from the post-finalize() shadow state. Used by the L8
    // depth quantity-bound invariant in test_invariants.cpp.
    [[nodiscard]] Quantity resting_qty_by_side(Symbol sym, Side side) const noexcept {
        Quantity total = 0;
        for (const auto& [id, s] : orders_) {
            if (s.currently_resting && s.symbol == sym && s.side == side) {
                total += (s.qty_submitted - s.qty_filled);
            }
        }
        return total;
    }

private:
    std::unordered_map<OrderId, ShadowOrder> orders_;
    std::vector<OrderId> submitted_in_order_;
};

struct EngineRun {
    std::vector<ExecutionReport> reports;
    // Per-event report-count slice so an invariant check that needs to
    // know which reports came from which event can find them.
    std::vector<std::size_t> report_count_per_event;
    ShadowTracker shadow;
    // The OrderIds that the engine itself believed were resting at the
    // end of the run, taken from OrderIndex. The "no lost orders"
    // invariant cross-checks this against the shadow's resting set.
    std::unordered_set<OrderId> engine_resting_ids;
    // For the IOC-never-rests invariant: the order types of every
    // currently-resting id at end of run, queried from the engine.
    std::unordered_map<OrderId, OrderType> engine_resting_types;

    // The set of demo symbols the run exercised. Invariants iterate
    // this list to walk Book::depth() per symbol.
    std::vector<Symbol> symbols;

    // Engine state held alive past run_engine() so post-run inspection
    // of Book::depth() and other accessors works. The order of these
    // members matters: pool, registry, index must outlive engine
    // because engine holds references to them.
    std::unique_ptr<OrderPool>      pool;
    std::unique_ptr<BookRegistry>   registry;
    std::unique_ptr<OrderIndex>     index;
    std::unique_ptr<MatchingEngine> engine;

    // Non-copyable, non-movable: MatchingEngine holds references to
    // the other three members; moving the EngineRun would invalidate
    // those references.
    EngineRun() = default;
    EngineRun(const EngineRun&) = delete;
    EngineRun& operator=(const EngineRun&) = delete;
    EngineRun(EngineRun&&) = delete;
    EngineRun& operator=(EngineRun&&) = delete;
};

inline std::unique_ptr<EngineRun> run_engine(const std::vector<GeneratedEvent>& events) {
    auto run = std::make_unique<EngineRun>();
    // Pool capacity: at most one resting order per submitted id, plus
    // generous slack for transient peaks during a sweep.
    run->pool = std::make_unique<OrderPool>(2 * events.size() + 64);
    run->registry = std::unique_ptr<BookRegistry>(new BookRegistry{
        kDemoSymbols[0], kDemoSymbols[1], kDemoSymbols[2],
        kDemoSymbols[3], kDemoSymbols[4]});
    run->index = std::make_unique<OrderIndex>();
    run->engine = std::make_unique<MatchingEngine>(*run->pool, *run->registry, *run->index);
    run->symbols = std::vector<Symbol>(kDemoSymbols.begin(), kDemoSymbols.end());

    run->report_count_per_event.reserve(events.size());
    for (const auto& g : events) {
        run->shadow.on_event(g.ev);
        auto reports = run->engine->apply(g.ev);
        run->report_count_per_event.push_back(reports.size());
        for (const auto& r : reports) {
            run->shadow.on_report(r);
            run->reports.push_back(r);
        }
    }
    run->shadow.finalize();
    // Snapshot the engine's view of resting ids at end of run by
    // querying every submitted id against the OrderIndex. This is the
    // ground truth for the "no lost orders" and "IOC never rests"
    // invariants.
    for (OrderId id : run->shadow.submitted_in_order()) {
        Order* o = run->index->find(id);
        if (o != nullptr) {
            run->engine_resting_ids.insert(id);
            run->engine_resting_types[id] = o->type;
        }
    }
    return run;
}

}  // namespace meridian::property
