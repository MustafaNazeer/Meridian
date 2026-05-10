#pragma once

// NASDAQ ITCH 5.0 binary tape parser, hand rolled, header only, bounds
// checked.
//
// Format
// ------
//
// Each message in the binary file format is preceded by a 2-byte
// big-endian length prefix. The length is the message body size
// including the 1-byte type field. So an Add Order ('A') message has
// length 36, an Order Delete ('D') has length 19, a System Event
// ('S') has length 12, and so on.
//
// Scope (this milestone)
// ----------------------
//
// Decoded:
//
//   * 'A' Add Order, no MPID attribution. Mapped to NewOrder Limit.
//   * 'D' Order Delete. Mapped to Cancel.
//
// Recognized but skipped:
//
//   * 'S' System Event. We honour its length but do not deliver it
//     to the caller (the start- / end-of-day markers are not
//     interesting for the matching engine path).
//   * Any other type. Skipped by the length prefix; we rely on the
//     prefix to step over messages we have not implemented decoders
//     for. This is the defensive parsing posture documented in
//     docs/security/threat-model.md and docs/security/checklist.md.
//
// Out of scope (deferred):
//
//   * 'F' Add Order with MPID. Treat as 'A' once needed; MPID can
//     be ignored for matching-engine purposes.
//   * 'X' Order Cancel (partial). The engine has no partial-cancel
//     API today; landing this message type requires both parser and
//     engine work.
//   * 'U' Order Replace. Modeled as Cancel + new Add at new px/qty.
//   * 'E', 'C' Order Executed. ITCH-outbound only; the engine
//     produces fills itself, not from the wire.
//
// Safety
// ------
//
// All field reads are bounds checked against the remaining buffer
// length. A truncated message returns ParseStatus::TruncatedFrame and
// leaves the cursor positioned at the start of the offending frame so
// the caller can inspect or stop. No exceptions are thrown.
//
// Pricing
// -------
//
// ITCH price fields are uint32 in 1/10000 dollar units (4 implicit
// decimal places). Meridian's Price type is int32 ticks. The parser
// passes the raw ITCH price field through as-is; one ITCH tick equals
// one Meridian tick. The integration test and the tape generator agree
// on this convention; downstream tooling that wants dollars multiplies
// by 1e-4.

#include "meridian/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace meridian::itch {

// 8-character left-justified, space-padded stock symbol from the ITCH
// wire format. Stored as a std::array so it is trivially copyable and
// comparable.
struct Stock {
    std::array<char, 8> data{};

    [[nodiscard]] friend bool operator==(const Stock&, const Stock&) = default;
};

// Decoded Add Order ('A') message body.
struct AddOrderMsg {
    Timestamp ts_ns;       // nanoseconds since midnight, from the 6-byte ITCH timestamp
    OrderId order_ref;     // ITCH Order Reference Number (uint64)
    Side side;             // 'B' -> Buy, 'S' -> Sell
    Quantity shares;       // ITCH Shares field (uint32)
    Stock stock;           // 8-char stock symbol
    Price price;           // raw ITCH price field, treated as integer ticks
};

// Decoded Order Delete ('D') message body.
struct OrderDeleteMsg {
    Timestamp ts_ns;
    OrderId order_ref;
};

enum class MessageKind : std::uint8_t {
    AddOrder,
    OrderDelete,
    OtherSkipped,  // recognized type that the caller does not need to act on
};

// Parser output. Both sub-structs are POD; the unused one is left
// default-initialized, which keeps the type trivially copyable without
// needing std::variant or a hand-rolled union.
struct ParsedMessage {
    MessageKind kind;
    AddOrderMsg add;        // valid iff kind == AddOrder
    OrderDeleteMsg del;     // valid iff kind == OrderDelete
};

enum class ParseStatus : std::uint8_t {
    Ok,
    EndOfStream,
    TruncatedFrame,    // 2-byte length prefix says N but fewer than N bytes follow
    InvalidLength,     // length prefix is shorter than the type byte (i.e. < 1)
    LengthMismatch,    // a recognized type's length does not match the spec
};

namespace detail {

constexpr std::uint16_t be16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>((std::uint16_t{p[0]} << 8) | std::uint16_t{p[1]});
}
constexpr std::uint32_t be32(const std::uint8_t* p) noexcept {
    return (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16) |
           (std::uint32_t{p[2]} << 8)  |  std::uint32_t{p[3]};
}
constexpr std::uint64_t be48(const std::uint8_t* p) noexcept {
    return (std::uint64_t{p[0]} << 40) | (std::uint64_t{p[1]} << 32) |
           (std::uint64_t{p[2]} << 24) | (std::uint64_t{p[3]} << 16) |
           (std::uint64_t{p[4]} << 8)  |  std::uint64_t{p[5]};
}
constexpr std::uint64_t be64(const std::uint8_t* p) noexcept {
    return (std::uint64_t{p[0]} << 56) | (std::uint64_t{p[1]} << 48) |
           (std::uint64_t{p[2]} << 40) | (std::uint64_t{p[3]} << 32) |
           (std::uint64_t{p[4]} << 24) | (std::uint64_t{p[5]} << 16) |
           (std::uint64_t{p[6]} << 8)  |  std::uint64_t{p[7]};
}

}  // namespace detail

// Documented ITCH 5.0 message body sizes (including the type byte).
inline constexpr std::uint16_t kAddOrderSize    = 36;
inline constexpr std::uint16_t kOrderDeleteSize = 19;
inline constexpr std::uint16_t kSystemEventSize = 12;

class Parser {
public:
    Parser(const std::uint8_t* buf, std::size_t len) noexcept
        : buf_(buf), end_(buf + len), cursor_(buf) {}

    [[nodiscard]] std::size_t bytes_remaining() const noexcept {
        return static_cast<std::size_t>(end_ - cursor_);
    }

    [[nodiscard]] std::size_t bytes_consumed() const noexcept {
        return static_cast<std::size_t>(cursor_ - buf_);
    }

    // Read the next message from the stream. On success, advances the
    // cursor past the consumed frame. On any non-Ok return, the cursor
    // is left at the start of the offending frame so the caller can
    // diagnose the failing offset.
    ParseStatus next(ParsedMessage& out) noexcept {
        if (cursor_ >= end_) {
            return ParseStatus::EndOfStream;
        }
        if (end_ - cursor_ < 2) {
            return ParseStatus::TruncatedFrame;
        }
        const std::uint16_t len = detail::be16(cursor_);
        if (len < 1) {
            return ParseStatus::InvalidLength;
        }
        if (static_cast<std::size_t>(end_ - cursor_) < std::size_t{2} + len) {
            return ParseStatus::TruncatedFrame;
        }
        const std::uint8_t* body = cursor_ + 2;
        const std::uint8_t type = body[0];

        switch (type) {
            case 'A': {
                if (len != kAddOrderSize) return ParseStatus::LengthMismatch;
                out.kind = MessageKind::AddOrder;
                out.add.ts_ns    = static_cast<Timestamp>(detail::be48(body + 5));
                out.add.order_ref = detail::be64(body + 11);
                const std::uint8_t bs = body[19];
                if (bs != 'B' && bs != 'S') return ParseStatus::LengthMismatch;
                out.add.side     = (bs == 'B') ? Side::Buy : Side::Sell;
                out.add.shares   = static_cast<Quantity>(detail::be32(body + 20));
                for (std::size_t i = 0; i < 8; ++i) {
                    out.add.stock.data[i] = static_cast<char>(body[24 + i]);
                }
                out.add.price    = static_cast<Price>(detail::be32(body + 32));
                break;
            }
            case 'D': {
                if (len != kOrderDeleteSize) return ParseStatus::LengthMismatch;
                out.kind = MessageKind::OrderDelete;
                out.del.ts_ns     = static_cast<Timestamp>(detail::be48(body + 5));
                out.del.order_ref = detail::be64(body + 11);
                break;
            }
            case 'S': {
                if (len != kSystemEventSize) return ParseStatus::LengthMismatch;
                out.kind = MessageKind::OtherSkipped;
                break;
            }
            default:
                // Unknown or out-of-scope type. Skip over the body
                // using the length prefix; do not abort. This matches
                // the defensive parsing posture documented in
                // docs/security/threat-model.md.
                out.kind = MessageKind::OtherSkipped;
                break;
        }
        cursor_ += std::size_t{2} + len;
        return ParseStatus::Ok;
    }

private:
    const std::uint8_t* buf_;
    const std::uint8_t* end_;
    const std::uint8_t* cursor_;
};

}  // namespace meridian::itch
