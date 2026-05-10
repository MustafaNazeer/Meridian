#pragma once

#include "meridian/types.hpp"

namespace meridian {

enum class ReportKind : std::uint8_t {
    Acknowledge = 0,
    Fill = 1,
    Cancel = 2,
    Reject = 3,
};

enum class RejectReason : std::uint8_t {
    None = 0,
    EmptyBook = 1,
    NotFound = 2,
    InsufficientLiquidity = 3,
    WouldCross = 4,
    UnknownSymbol = 5,
};

// Per docs/risk/matching-semantics.md (Quant Domain Validator, 2026-05-09):
// each match emits TWO Fill reports back to back (maker first, then taker).
// Each report is self-describing about its own side. Acknowledge precedes
// any fills against an accepted NewOrder. Outright rejects (market against
// empty book) emit only Reject. Cancel events emit Cancel (success) or
// Reject NotFound (unknown id); no Acknowledge for cancels.
struct ExecutionReport {
    ReportKind kind;
    OrderId order_id;
    Side side;
    Price price;
    Quantity qty;
    Timestamp ts;
    RejectReason reject_reason;
};

}  // namespace meridian
