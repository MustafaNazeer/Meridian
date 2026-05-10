// Property-based tests for the matching engine's single-symbol invariants
// (and their multi-symbol generalisation, since the generator drives the
// demo symbol set 1..5 across the same registry).
//
// Each TEST in this file:
//   1. Iterates over kCases distinct seeds (kSeedBase + i).
//   2. Generates a random EngineEvent sequence from each seed.
//   3. Runs the sequence through a fresh MatchingEngine.
//   4. Checks one invariant against the report stream and the engine's
//      end-of-run state.
//
// On failure the assertion message includes the seed, so a regression
// can be reproduced by hardcoding that seed in a one-off scenario.
//
// The five invariants below are the in-scope set from
// docs/risk/matching-semantics.md section 7.

#include "harness.hpp"
#include "meridian/types.hpp"

#include <cstdint>
#include <deque>
#include <gtest/gtest.h>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace meridian::property {
namespace {

constexpr int kCases = 1000;
constexpr std::uint64_t kSeedBase = 0xA17021A4u;  // arbitrary, fixed

// Invariant 1: every submitted OrderId satisfies
//   submitted == filled + resting + cancelled
// where "cancelled" includes both explicit Cancel reports and the
// implicit cancellation of unfilled residual on Market/IOC orders, and
// rejected-at-submission orders satisfy filled == 0, resting == 0,
// cancelled == 0 (the engine treats them as if they never existed past
// the Reject report).
TEST(MatchingInvariants, QuantityConservation) {
    for (int i = 0; i < kCases; ++i) {
        const std::uint64_t seed = kSeedBase + static_cast<std::uint64_t>(i);
        SCOPED_TRACE(::testing::Message() << "seed=" << seed);
        auto events = EventGenerator(seed).generate();
        auto run = run_engine(events);
        for (const auto& [id, s] : run.shadow.orders()) {
            if (!s.submitted) continue;
            if (s.rejected_at_submission) {
                EXPECT_EQ(s.qty_filled, 0) << "rejected order id " << id
                                           << " should not have any fills";
                EXPECT_FALSE(s.cancelled) << "rejected order id " << id
                                          << " should not get a cancel report";
                continue;
            }
            const Quantity resting   = run.shadow.resting_qty(s);
            const Quantity implicit  = run.shadow.implicit_cancel_qty(s);
            const Quantity cancelled = (s.cancelled ? s.qty_cancelled : 0) + implicit;
            const Quantity sum = s.qty_filled + resting + cancelled;
            EXPECT_EQ(sum, s.qty_submitted)
                << "conservation broken for id " << id
                << ": submitted=" << s.qty_submitted
                << " filled=" << s.qty_filled
                << " resting=" << resting
                << " cancelled=" << cancelled;
        }
    }
}

// Invariant 2: no OrderId ever receives a stream of Fill reports whose
// cumulative quantity exceeds its submitted quantity.
TEST(MatchingInvariants, NoDoubleFill) {
    for (int i = 0; i < kCases; ++i) {
        const std::uint64_t seed = kSeedBase + static_cast<std::uint64_t>(i);
        SCOPED_TRACE(::testing::Message() << "seed=" << seed);
        auto events = EventGenerator(seed).generate();
        auto run = run_engine(events);
        for (const auto& [id, s] : run.shadow.orders()) {
            if (!s.submitted) continue;
            EXPECT_LE(s.qty_filled, s.qty_submitted)
                << "double fill on id " << id
                << ": submitted=" << s.qty_submitted
                << " cumulative_filled=" << s.qty_filled;
        }
    }
}

// Invariant 3: every submitted OrderId is in exactly one of these
// terminal-or-resting states:
//   * filled              (no remaining qty, all filled)
//   * partially_resting   (some filled, some resting) - Limit only
//   * fully_resting       (no fills, full qty resting) - Limit only
//   * cancelled_no_fills  (no fills, then cancelled)
//   * cancelled_partial   (some fills, then cancelled)
//   * implicit_cancel     (Market/IOC residual was implicitly cancelled)
//   * rejected            (submission rejected; no further activity)
// The shadow tracker reconstructs this state from the report stream.
// The engine's OrderIndex view (engine_resting_ids) must agree with the
// shadow's resting set, otherwise an order is "lost" in one view or
// duplicated across two states.
TEST(MatchingInvariants, NoLostOrders) {
    for (int i = 0; i < kCases; ++i) {
        const std::uint64_t seed = kSeedBase + static_cast<std::uint64_t>(i);
        SCOPED_TRACE(::testing::Message() << "seed=" << seed);
        auto events = EventGenerator(seed).generate();
        auto run = run_engine(events);
        std::unordered_set<OrderId> shadow_resting;
        for (const auto& [id, s] : run.shadow.orders()) {
            if (!s.submitted) continue;

            const bool filled       = (s.qty_filled == s.qty_submitted) &&
                                      !s.cancelled && !s.rejected_at_submission;
            const bool fully_rest   = s.currently_resting && s.qty_filled == 0;
            const bool partial_rest = s.currently_resting && s.qty_filled > 0;
            const bool cancel_none  = s.cancelled && s.qty_filled == 0;
            const bool cancel_part  = s.cancelled && s.qty_filled > 0 &&
                                      s.qty_filled < s.qty_submitted;
            const bool implicit     = !s.cancelled && !s.rejected_at_submission &&
                                      !s.currently_resting &&
                                      s.qty_filled < s.qty_submitted &&
                                      (s.type == OrderType::Market ||
                                       s.type == OrderType::IOC);
            const bool rejected     = s.rejected_at_submission;

            const int hits = filled + fully_rest + partial_rest + cancel_none +
                             cancel_part + implicit + rejected;
            EXPECT_EQ(hits, 1)
                << "id " << id << " is in " << hits
                << " states at end of sequence";
            if (s.currently_resting) shadow_resting.insert(id);
        }
        EXPECT_EQ(shadow_resting.size(), run.engine_resting_ids.size())
            << "shadow says " << shadow_resting.size() << " resting, "
            << "engine OrderIndex says " << run.engine_resting_ids.size();
        for (OrderId id : shadow_resting) {
            EXPECT_TRUE(run.engine_resting_ids.contains(id))
                << "shadow says id " << id
                << " is resting; engine OrderIndex disagrees";
        }
    }
}

// Invariant 4: no OrderId submitted with type IOC is ever found in the
// engine's OrderIndex after the matching loop returns. (The shadow's
// finalize() also enforces this for Market/IOC residuals via
// implicit_cancel_qty, but the engine-side OrderIndex query is the
// stronger statement.)
TEST(MatchingInvariants, IocNeverRests) {
    for (int i = 0; i < kCases; ++i) {
        const std::uint64_t seed = kSeedBase + static_cast<std::uint64_t>(i);
        SCOPED_TRACE(::testing::Message() << "seed=" << seed);
        auto events = EventGenerator(seed).generate();
        auto run = run_engine(events);
        for (const auto& [id, type] : run.engine_resting_types) {
            EXPECT_NE(type, OrderType::IOC)
                << "IOC order " << id << " is resting in OrderIndex";
            EXPECT_NE(type, OrderType::Market)
                << "Market order " << id << " is resting in OrderIndex";
        }
    }
}

// Invariant 5: at each (symbol, side, price) level, fills consume the
// FIFO queue from front to back in insertion order. This is the
// behavioural form of price-time priority: the structural form (the
// intrusive linked list ordered by ts_arrival) is enforced by Book and
// Level; this test verifies the engine actually emits fills against the
// front of the queue first.
//
// Implementation: replay the event stream alongside the engine. Maintain
// our own per-(symbol, side, price) FIFO queue of resting OrderIds and
// per-OrderId remaining quantity. For every Fill report whose order_id
// is a known resting maker, assert it is at the front of its level's
// queue and decrement its remaining; pop on full fill. For Cancel
// reports, remove the id from wherever it sits in its queue. When a new
// Limit order's reports finish, append its residual to the back of its
// level if any qty remains.
TEST(MatchingInvariants, PriceTimePriority) {
    using Level = std::tuple<Symbol, Side, Price>;
    struct LevelHash {
        std::size_t operator()(const Level& l) const noexcept {
            const std::size_t a = std::hash<Symbol>{}(std::get<0>(l));
            const std::size_t b = std::hash<int>{}(static_cast<int>(std::get<1>(l)));
            const std::size_t c = std::hash<Price>{}(std::get<2>(l));
            return a ^ (b << 1) ^ (c << 2);
        }
    };

    for (int i = 0; i < kCases; ++i) {
        const std::uint64_t seed = kSeedBase + static_cast<std::uint64_t>(i);
        SCOPED_TRACE(::testing::Message() << "seed=" << seed);
        auto events = EventGenerator(seed).generate();
        auto run = run_engine(events);

        struct RestingState {
            Symbol symbol;
            Side side;
            Price price;
            Quantity qty_remaining;
        };
        std::unordered_map<OrderId, RestingState> resting;
        std::unordered_map<Level, std::deque<OrderId>, LevelHash> queues;

        std::size_t report_idx = 0;
        for (std::size_t ei = 0; ei < events.size(); ++ei) {
            const EngineEvent& ev = events[ei].ev;
            const std::size_t n = run.report_count_per_event[ei];
            const std::size_t event_first_report = report_idx;
            (void)event_first_report;

            // Track filled qty for the aggressor to know whether the
            // event's NewOrder Limit had residual that rested.
            Quantity aggressor_filled = 0;

            for (std::size_t ri = 0; ri < n; ++ri) {
                const ExecutionReport& r = run.reports[report_idx++];
                if (r.kind == ReportKind::Fill) {
                    auto it = resting.find(r.order_id);
                    if (it != resting.end()) {
                        // Maker fill. Check FIFO front, decrement, pop on full.
                        const RestingState& st = it->second;
                        Level key{st.symbol, st.side, st.price};
                        auto qit = queues.find(key);
                        ASSERT_NE(qit, queues.end())
                            << "fill against an empty FIFO queue at level";
                        ASSERT_EQ(qit->second.front(), r.order_id)
                            << "fill on maker " << r.order_id
                            << " at (sym=" << st.symbol << ", side="
                            << static_cast<int>(st.side) << ", px=" << st.price
                            << ") but FIFO front is " << qit->second.front();
                        ASSERT_LE(r.qty, it->second.qty_remaining)
                            << "fill qty exceeds maker remaining";
                        it->second.qty_remaining -= r.qty;
                        if (it->second.qty_remaining == 0) {
                            qit->second.pop_front();
                            if (qit->second.empty()) queues.erase(qit);
                            resting.erase(it);
                        }
                    } else {
                        // Aggressor fill. Sum into aggressor_filled.
                        if (ev.kind == EventKind::NewOrder &&
                            r.order_id == ev.order_id) {
                            aggressor_filled += r.qty;
                        }
                    }
                } else if (r.kind == ReportKind::Cancel) {
                    auto it = resting.find(r.order_id);
                    if (it != resting.end()) {
                        const RestingState& st = it->second;
                        Level key{st.symbol, st.side, st.price};
                        auto qit = queues.find(key);
                        ASSERT_NE(qit, queues.end())
                            << "cancel for id " << r.order_id
                            << " whose level queue is empty";
                        // Remove the id from anywhere in the deque.
                        auto& q = qit->second;
                        bool removed = false;
                        for (auto dit = q.begin(); dit != q.end(); ++dit) {
                            if (*dit == r.order_id) {
                                q.erase(dit);
                                removed = true;
                                break;
                            }
                        }
                        ASSERT_TRUE(removed)
                            << "cancel for id " << r.order_id
                            << " not found in its level queue";
                        if (q.empty()) queues.erase(qit);
                        resting.erase(it);
                    }
                    // If id was not resting in our shadow, it must be
                    // a NotFound reject in this position; the engine
                    // emits Cancel only for known resting ids.
                }
                // Acknowledge / Reject have no effect on the FIFO queue.
            }

            // After this event's reports, if the event was a Limit
            // NewOrder and there is residual, append the id to its
            // (symbol, side, price) FIFO queue.
            if (ev.kind == EventKind::NewOrder &&
                ev.type == OrderType::Limit) {
                const Quantity residual = ev.qty - aggressor_filled;
                if (residual > 0) {
                    Level key{ev.symbol, ev.side, ev.price};
                    queues[key].push_back(ev.order_id);
                    resting[ev.order_id] = RestingState{
                        ev.symbol, ev.side, ev.price, residual};
                }
            }
        }
        // End of run: every id that the engine reports as resting must
        // also be tracked as resting in this shadow at the same level.
        ASSERT_EQ(resting.size(), run.engine_resting_ids.size())
            << "shadow tracks " << resting.size()
            << " resting orders, engine has " << run.engine_resting_ids.size();
        for (OrderId id : run.engine_resting_ids) {
            ASSERT_TRUE(resting.contains(id))
                << "engine reports id " << id
                << " resting; shadow does not";
        }
    }
}

}  // namespace
}  // namespace meridian::property
