#pragma once

#include <cstdint>

namespace meridian {

using OrderId = std::uint64_t;
using Symbol = std::uint16_t;
using Price = std::int32_t;
using Quantity = std::int32_t;
using Timestamp = std::int64_t;

inline constexpr Price kInvalidPrice = -1;
inline constexpr Quantity kInvalidQuantity = -1;
inline constexpr OrderId kInvalidOrderId = 0;

enum class Side : std::uint8_t {
    Buy = 0,
    Sell = 1,
};

enum class OrderType : std::uint8_t {
    Limit = 0,
    Market = 1,
    IOC = 2,
    PostOnly = 3,
    FOK = 4,
};

struct Order {
    OrderId id;
    Symbol symbol;
    Side side;
    OrderType type;
    Price price;
    Quantity qty_remaining;
    Timestamp ts_arrival;
    Order* prev;
    Order* next;
};

static_assert(sizeof(Order) <= 64,
              "Order must fit in one 64-byte cache line");

enum class EventKind : std::uint8_t {
    NewOrder = 0,
    Cancel = 1,
};

struct EngineEvent {
    EventKind kind;
    Symbol symbol;
    Timestamp ts;
    OrderId order_id;
    Side side;
    OrderType type;
    Price price;
    Quantity qty;
};

}  // namespace meridian
