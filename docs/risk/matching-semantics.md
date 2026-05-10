# Matching Semantics: Limit, Market, and IOC

**Owner**: Quant Domain Validator (agent 01)
**Phase**: 1 (Core data structures and single-symbol matching)
**Status**: Draft, awaiting Citation and Fact Auditor pass
**Date**: 2026-05-09

This document is the canonical conventions reference for the Meridian matching engine's Phase 1 behavior. It defines, in worked-example form, exactly what the engine must produce for each event in the Phase 1 scope: **limit**, **market**, and **IOC** new-order events plus **cancel-by-id**. Post-only and FOK are explicitly out of scope for this document; they land in Phase 7 with a follow-up extension to this file.

The Reference Implementation Engineer (agent 18) implements `tests/reference/matching_reference.py` so that every worked example below passes as a unit test. The Engine Developer / Market Microstructure Engineer (agents 02 and 03, played by the PM session in Phase 1) implements `libmeridian.a` so that its execution-report stream matches the reference byte for byte on every input. The QA Engineer (agent 10) turns the invariant checklist in section 7 into property tests in Phase 3.

---

## 1. Type widths and naming

The names and widths below are locked by the Phase 1 companion plan ("Key decisions captured for Phase 1", section 5) at `docs/superpowers/plans/2026-05-09-phase-1-single-symbol-matching.md`. Every worked example uses these exact names.

| Name | Type | Notes |
|---|---|---|
| `OrderId` | `uint64_t` | Engine-assigned unique identifier per submitted order. |
| `Symbol` | `uint16_t` | Symbol id, single-symbol scope in Phase 1. Worked examples use `Symbol = 1`. |
| `Side` | `enum class : uint8_t { Buy, Sell }` | Aggressor side; resting orders carry the same side. |
| `OrderType` | `enum class : uint8_t { Limit, Market, IOC, PostOnly, FOK }` | `PostOnly` and `FOK` declared but not implemented in Phase 1. |
| `Price` | `int32_t` | Integer ticks. Signed to reserve negatives as sentinels. |
| `Quantity` | `int32_t` | Integer share count. Signed but never negative in valid events. |
| `Timestamp` | `int64_t` | Nanoseconds since epoch. Worked examples use small integers (`ts=1, 2, 3, ...`) for clarity; the engine treats these as opaque monotonically nondecreasing values. |
| `ReportKind` | `enum class : uint8_t { Acknowledge, Fill, Reject, Cancel }` | The four execution-report kinds emitted in Phase 1. |
| `RejectReason` | `enum class : uint8_t { EmptyBook, NotFound, ... }` | Phase 1 uses `EmptyBook` (market against empty side) and `NotFound` (cancel of unknown id). Other reasons (`WouldCross`, `InsufficientLiquidity`) are reserved for Phase 7 and later. |
| `ExecutionReport` | struct | Fields: `kind`, `order_id`, `side`, `price`, `qty`, `ts`, plus `reason` (only when `kind == Reject`). |
| `EngineEvent` | tagged union | `NewOrder` (carries `OrderType`, `Side`, `Price`, `Quantity`, `OrderId`, `Timestamp`) and `Cancel` (carries `OrderId`, `Timestamp`). |

Notation in the worked examples:

* `B` is shorthand for `Side::Buy`, `S` for `Side::Sell`.
* `LIM`, `MKT`, `IOC` are shorthand for `OrderType::Limit`, `OrderType::Market`, `OrderType::IOC`.
* Prices are integer ticks. Worked examples use values around `10000` to keep the arithmetic simple.
* "Book state" snapshots are written as two columns (Bids descending, Asks ascending), with each level rendered as `price x [orderid:qty, orderid:qty, ...]` where the leftmost order in the bracket is the front of the FIFO queue (the order that has rested longest).
* Trades are reported as **two** Fill reports (one for the resting maker, one for the incoming taker), both with the same `ts` (the aggressor's `ts_arrival`) and the same `price` (the resting maker's price, i.e., the trade price). The two fills carry the maker's and the taker's respective `order_id` and `side`.

---

## 2. Price-time priority and the matching loop (textbook rules)

### 2.1 Price priority

A bid at a higher price has priority over a bid at a lower price. An ask at a lower price has priority over an ask at a higher price. An aggressing buy order seeking liquidity walks the ask side from the lowest ask price upward, taking each price level in turn until either the order's quantity is exhausted, the order's price limit is reached (for limit and IOC), or the opposite side is empty. An aggressing sell order walks the bid side from the highest bid price downward, symmetrically.

This rule is the standard "price priority" of a continuous limit order book. See Harris, *Trading and Exchanges: Market Microstructure for Practitioners* (Oxford University Press, 2003), Chapter 6 ("Order-Driven Markets") section on order precedence rules. See also O'Hara, *Market Microstructure Theory* (Blackwell, 1995), Chapter 1 for the formal definition of a limit order book as a sorted collection of price levels with FIFO queues at each level. [citation needed: confirm exact section number for Harris and O'Hara]

### 2.2 Time priority (tie-breaking at a price level)

**At a single price level, the order resting longer (lower `ts_arrival`) trades first against an incoming aggressor.** This is FIFO at the level: the front of the queue is the oldest resting order; the tail is the most recent insertion. New limit orders that come to rest at a price level are appended at the tail and have the lowest priority at that level until a closer-to-the-front order is fully filled or cancelled.

This is the "time priority" half of price-time priority. See Harris, *Trading and Exchanges*, Chapter 6 on order precedence. The NASDAQ TotalView ITCH 5.0 specification (NASDAQ TotalView-ITCH 5.0 specification, document version 5.0) describes price-time priority as the matching discipline implicitly through its `Order Executed` and `Trade` message semantics. [citation needed: confirm Harris chapter and ITCH spec page reference for the explicit price-time wording]

### 2.3 The matching loop in plain English

When a `NewOrder` event arrives:

1. If the order is `Market`, attempt to take liquidity at any available price on the opposite side (no price limit).
2. If the order is `Limit` or `IOC`, walk the opposite side starting from the most aggressive resting price (lowest ask if aggressor is `Buy`, highest bid if aggressor is `Sell`). At each price level, while the level's price is on the aggressor's "willing to trade" side of its limit, fill against the front of the level's FIFO queue.
3. A fill consumes `min(aggressor_remaining_qty, maker_remaining_qty)` shares at the maker's price. Both the maker's and the taker's remaining quantities decrease by that amount. If the maker is fully consumed, it is unlinked from the level and removed from the per-symbol `OrderId -> Order*` lookup map. If the level becomes empty, it is removed from the side's price map.
4. The aggressor stops walking when (a) its remaining quantity reaches zero, (b) the next maker's price violates its limit (limit and IOC only), or (c) the opposite side is empty.
5. After walking:
   * **Limit**: if the aggressor has remaining quantity, the residual is inserted at the tail of the FIFO queue at its limit price on its own side, and an `Acknowledge` report is emitted at the start of the report stream (before any fills).
   * **Market**: if the aggressor has remaining quantity but the opposite side is empty, the residual is implicitly cancelled (no report). Market orders never rest.
   * **IOC**: if the aggressor has remaining quantity, the residual is implicitly cancelled (no report). IOC orders never rest.
6. The very first report emitted for any new order is an `Acknowledge` (kind `Acknowledge`, with the original submitted quantity), regardless of whether the order rests, partially fills, fully fills, or is rejected outright. The acknowledge precedes all fills for that order. This makes the report stream self-describing: a downstream consumer always sees an ack before any fills it has to attribute.

When a `Cancel` event arrives:

1. Look up the `OrderId` in the per-symbol `OrderId -> Order*` map.
2. If the order is found, unlink it from its level, remove it from the lookup map, possibly remove the now-empty level from the side, and emit a `Cancel` report carrying the cancelled order's remaining quantity.
3. If the order is not found (either never existed, already fully filled, or already cancelled), emit a `Reject` report with `reason = NotFound`. See section 6 on cancel-by-id for the worked examples.

This single rulebook handles every Phase 1 worked example below.

---

## 3. Limit order semantics

A `Limit` order specifies a price at which the submitter is willing to trade. On arrival:

1. If the order would cross (a buy at `>=` the best ask, or a sell at `<=` the best bid), it consumes liquidity from the opposite side as described in section 2.3 until either it fully fills, its limit is reached, or the opposite side is empty.
2. Any residual quantity rests on the order's own side at the limit price, joining the tail of that level's FIFO queue.
3. An `Acknowledge` report is always emitted first (before any fills), with the original submitted quantity.

### 3.1 Worked example LIM-1: passive limit on an empty book

**Initial book**: empty (no bids, no asks).

**Event**: `NewOrder { id=101, side=B, type=LIM, price=10000, qty=50, ts=1 }`

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=101, side=B, price=10000, qty=50, ts=1 }`

**Post-event book**:

```
Bids:                         Asks:
10000 x [101:50]              (empty)
```

**Notes**: the order does not cross because there is no opposite side to cross against. It rests passively at price 10000. Submitted=50, filled=0, resting=50, cancelled=0.

### 3.2 Worked example LIM-2: passive limit joining the tail at an existing level (FIFO)

**Initial book**:

```
Bids:                         Asks:
10000 x [101:50]              (empty)
```

**Event**: `NewOrder { id=102, side=B, type=LIM, price=10000, qty=30, ts=2 }`

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=102, side=B, price=10000, qty=30, ts=2 }`

**Post-event book**:

```
Bids:                         Asks:
10000 x [101:50, 102:30]      (empty)
```

**Notes**: 102 joins the tail of the price level 10000. Order 101 remains at the front of the queue and will trade first against any future aggressor selling at 10000. Submitted=30, filled=0, resting=30, cancelled=0 for order 102.

### 3.3 Worked example LIM-3: aggressive limit fully crossing one resting order

**Initial book**:

```
Bids:                         Asks:
(empty)                       10001 x [201:40]
```

**Event**: `NewOrder { id=301, side=B, type=LIM, price=10001, qty=40, ts=3 }`

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=301, side=B, price=10001, qty=40, ts=3 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=40, ts=3 }` (maker side)
3. `{ kind=Fill, order_id=301, side=B, price=10001, qty=40, ts=3 }` (taker side)

**Post-event book**:

```
Bids:                         Asks:
(empty)                       (empty)
```

**Notes**: order 301 takes the entire resting ask at the trade price 10001 (maker's price). Both sides report a Fill at the same price and timestamp. Submitted=40, filled=40, resting=0, cancelled=0 for order 301.

### 3.4 Worked example LIM-4: aggressive limit sweeping two price levels with a residual that rests

**Initial book**:

```
Bids:                         Asks:
(empty)                       10001 x [201:30]
                              10002 x [202:40]
                              10003 x [203:50]
```

**Event**: `NewOrder { id=302, side=B, type=LIM, price=10002, qty=100, ts=4 }`

The aggressor's limit price is 10002, so it is willing to fill against levels 10001 and 10002 only. Level 10003 is too expensive.

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=302, side=B, price=10002, qty=100, ts=4 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=30, ts=4 }` (maker, fully filled at 10001)
3. `{ kind=Fill, order_id=302, side=B, price=10001, qty=30, ts=4 }` (taker, partial)
4. `{ kind=Fill, order_id=202, side=S, price=10002, qty=40, ts=4 }` (maker, fully filled at 10002)
5. `{ kind=Fill, order_id=302, side=B, price=10002, qty=40, ts=4 }` (taker, partial)

After step 5, the aggressor has filled 70 of its 100 shares (30 at 10001, 40 at 10002). It still has 30 shares remaining and its limit price is 10002. The next ask level is 10003, which exceeds its limit, so it stops walking. The 30-share residual rests on the bid side at price 10002.

**Post-event book**:

```
Bids:                         Asks:
10002 x [302:30]              10003 x [203:50]
```

**Notes**: this example exercises the most common limit-order branch in real markets: partially fills as a taker, then rests as a maker for the residual. Submitted=100, filled=70, resting=30, cancelled=0 for order 302.

### 3.5 Worked example LIM-5: time priority within a level (FIFO is preserved)

**Initial book**:

```
Bids:                         Asks:
(empty)                       10001 x [201:20, 202:20, 203:20]
```

(Three sell orders at the same price 10001, inserted in the order 201, 202, 203 by `ts_arrival`. Order 201 is at the front of the FIFO queue.)

**Event**: `NewOrder { id=401, side=B, type=LIM, price=10001, qty=35, ts=5 }`

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=401, side=B, price=10001, qty=35, ts=5 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=20, ts=5 }` (front of queue, fully filled)
3. `{ kind=Fill, order_id=401, side=B, price=10001, qty=20, ts=5 }`
4. `{ kind=Fill, order_id=202, side=S, price=10001, qty=15, ts=5 }` (next in queue, partial fill)
5. `{ kind=Fill, order_id=401, side=B, price=10001, qty=15, ts=5 }`

The aggressor has now filled 35 shares. It stops because its quantity is exhausted.

**Post-event book**:

```
Bids:                         Asks:
(empty)                       10001 x [202:5, 203:20]
```

**Notes**: FIFO is the load-bearing rule. Order 201 trades before 202, and 202 before 203, because that is the order they were inserted. Order 202 is partially filled and remains at the front of the queue with 5 shares; order 203 stays behind it. This is the price-time priority invariant in its purest form.

### 3.6 Worked example LIM-6: aggressive limit sweeping two same-price-level orders before going to the next level

**Initial book**:

```
Bids:                         Asks:
(empty)                       10001 x [201:20, 202:20]
                              10002 x [203:50]
```

**Event**: `NewOrder { id=402, side=B, type=LIM, price=10002, qty=80, ts=6 }`

The aggressor at price 10002 is willing to take both levels.

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=402, side=B, price=10002, qty=80, ts=6 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=20, ts=6 }` (front of 10001 level)
3. `{ kind=Fill, order_id=402, side=B, price=10001, qty=20, ts=6 }`
4. `{ kind=Fill, order_id=202, side=S, price=10001, qty=20, ts=6 }` (next at 10001)
5. `{ kind=Fill, order_id=402, side=B, price=10001, qty=20, ts=6 }`
6. `{ kind=Fill, order_id=203, side=S, price=10002, qty=40, ts=6 }` (next level, partial fill)
7. `{ kind=Fill, order_id=402, side=B, price=10002, qty=40, ts=6 }`

The aggressor has filled 80 shares (20+20+40) and stops because its quantity is exhausted.

**Post-event book**:

```
Bids:                         Asks:
(empty)                       10002 x [203:10]
```

**Notes**: this exercises the matching loop's nested walk: first across orders within a price level, then across levels. The aggressor consumes the entire 10001 level before touching the 10002 level, even though both are within its limit. Submitted=80, filled=80, resting=0, cancelled=0 for order 402.

### 3.7 Worked example LIM-7: aggressive limit fully crossing then leaving zero residual (sells side)

**Initial book**:

```
Bids:                         Asks:
9999  x [101:60]              (empty)
9998  x [102:40]
```

**Event**: `NewOrder { id=501, side=S, type=LIM, price=9999, qty=60, ts=7 }`

The aggressor is selling at 9999. Its opposite side is the bid side. It walks bids from the highest price downward. Best bid is 9999 (order 101 with 60 shares), which matches the aggressor's limit. It is willing to trade.

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=501, side=S, price=9999, qty=60, ts=7 }`
2. `{ kind=Fill, order_id=101, side=B, price=9999, qty=60, ts=7 }` (maker)
3. `{ kind=Fill, order_id=501, side=S, price=9999, qty=60, ts=7 }` (taker)

**Post-event book**:

```
Bids:                         Asks:
9998 x [102:40]               (empty)
```

**Notes**: confirms that the matching rules are symmetric for sell aggressors. The 9998 level is below the aggressor's limit-of-9999 willingness (a sell at 9999 will not trade at 9998), so the aggressor stops after filling 60 at the 9999 level. Submitted=60, filled=60, resting=0, cancelled=0 for order 501. This example also demonstrates that a sell at limit `p` is willing to fill at any bid price `>= p` (taking the best bid first, walking downward to the limit but not below it).

---

## 4. Market order semantics

A `Market` order has no price limit. On arrival:

1. If the opposite side is empty, the engine emits a `Reject` report with `reason = EmptyBook` and the order is dropped. No `Acknowledge` is emitted. This is a deliberate convention choice for Phase 1; see section 4.1 for the rationale.
2. Otherwise, the order takes liquidity at any available price on the opposite side, walking from the most aggressive resting price as in section 2.3. The price limit is irrelevant; matching continues until either the aggressor's quantity is exhausted or the opposite side is empty.
3. If the aggressor's quantity is exhausted, the order is fully filled.
4. If the opposite side empties before the aggressor's quantity is exhausted, the residual is implicitly cancelled (no `Cancel` report). Market orders never rest.

### 4.1 Convention choice: market against empty book is a Reject (no Acknowledge first)

The textbook references give two acceptable behaviors for a market order against an empty book: (a) emit an `Acknowledge` followed immediately by a `Reject` with `reason = EmptyBook`, or (b) emit only a `Reject` and skip the `Acknowledge`. Real exchanges differ. This document picks **(b): a single `Reject` with `reason = EmptyBook` and no preceding `Acknowledge`**, on the grounds that the order never reached the matching loop in any meaningful sense and a downstream consumer correlating fills to acks gets a cleaner trace if rejected orders skip the ack entirely. The Reference Implementation Engineer must implement this convention; the C++ engine must agree.

This is one of the two convention choices flagged for PM review in the Quant Domain Validator's hand-off note (see section 8 of this document).

### 4.2 Worked example MKT-1: market buy fully filling against a single resting ask

**Initial book**:

```
Bids:                         Asks:
(empty)                       10001 x [201:50]
```

**Event**: `NewOrder { id=601, side=B, type=MKT, price=0, qty=50, ts=8 }`

(A market order's `price` field is unused; this document writes `price=0` to mark it as a sentinel. The reference and the C++ engine ignore the field.)

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=601, side=B, price=0, qty=50, ts=8 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=50, ts=8 }` (maker)
3. `{ kind=Fill, order_id=601, side=B, price=10001, qty=50, ts=8 }` (taker, at maker's price)

**Post-event book**:

```
Bids:                         Asks:
(empty)                       (empty)
```

**Notes**: the market order takes the resting ask at the maker's price 10001. Submitted=50, filled=50, resting=0, cancelled=0 for order 601.

### 4.3 Worked example MKT-2: market buy sweeping multiple price levels

**Initial book**:

```
Bids:                         Asks:
(empty)                       10001 x [201:20]
                              10002 x [202:30]
                              10005 x [203:40]
```

**Event**: `NewOrder { id=602, side=B, type=MKT, price=0, qty=80, ts=9 }`

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=602, side=B, price=0, qty=80, ts=9 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=20, ts=9 }`
3. `{ kind=Fill, order_id=602, side=B, price=10001, qty=20, ts=9 }`
4. `{ kind=Fill, order_id=202, side=S, price=10002, qty=30, ts=9 }`
5. `{ kind=Fill, order_id=602, side=B, price=10002, qty=30, ts=9 }`
6. `{ kind=Fill, order_id=203, side=S, price=10005, qty=30, ts=9 }` (partial fill at 10005)
7. `{ kind=Fill, order_id=602, side=B, price=10005, qty=30, ts=9 }`

**Post-event book**:

```
Bids:                         Asks:
(empty)                       10005 x [203:10]
```

**Notes**: the market order walks past a 3-tick gap (between 10002 and 10005) without flinching because it has no price limit. It ends fully filled at 80 shares total: 20 at 10001, 30 at 10002, 30 at 10005. The maker order 203 is partially filled and remains at the front of the 10005 level with 10 shares.

### 4.4 Worked example MKT-3: market buy partially filling and the residual implicitly cancelled

**Initial book**:

```
Bids:                         Asks:
(empty)                       10001 x [201:30]
```

**Event**: `NewOrder { id=603, side=B, type=MKT, price=0, qty=100, ts=10 }`

The book has only 30 shares to give. The market order takes them, then the opposite side is empty.

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=603, side=B, price=0, qty=100, ts=10 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=30, ts=10 }`
3. `{ kind=Fill, order_id=603, side=B, price=10001, qty=30, ts=10 }`

After step 3, the opposite side is empty and the aggressor has 70 shares remaining. The 70 shares are implicitly cancelled (no further report).

**Post-event book**:

```
Bids:                         Asks:
(empty)                       (empty)
```

**Notes**: this is the canonical "partial fill, implicit cancel of residual" branch for market orders. Submitted=100, filled=30, resting=0, cancelled=70 for order 603. The conservation law (section 7) holds: 100 = 30 + 0 + 70.

### 4.5 Worked example MKT-4: market buy against an empty book is rejected

**Initial book**: empty.

**Event**: `NewOrder { id=604, side=B, type=MKT, price=0, qty=50, ts=11 }`

**Reports emitted (in order)**:
1. `{ kind=Reject, order_id=604, side=B, price=0, qty=50, ts=11, reason=EmptyBook }`

**Post-event book**: empty (unchanged).

**Notes**: per the convention in section 4.1, no `Acknowledge` precedes the `Reject`. The `qty` carried on the `Reject` is the original submitted quantity (50), so a downstream consumer can see exactly what was rejected.

### 4.6 Worked example MKT-5: market sell fully filling against a single resting bid

**Initial book**:

```
Bids:                         Asks:
9999 x [101:40]               (empty)
```

**Event**: `NewOrder { id=605, side=S, type=MKT, price=0, qty=40, ts=12 }`

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=605, side=S, price=0, qty=40, ts=12 }`
2. `{ kind=Fill, order_id=101, side=B, price=9999, qty=40, ts=12 }` (maker)
3. `{ kind=Fill, order_id=605, side=S, price=9999, qty=40, ts=12 }` (taker)

**Post-event book**:

```
Bids:                         Asks:
(empty)                       (empty)
```

**Notes**: symmetric to MKT-1. Confirms that a market sell walks the bid side from the highest price down. Submitted=40, filled=40, resting=0, cancelled=0 for order 605.

### 4.7 Worked example MKT-6: market sell against an empty bid side is rejected

**Initial book**:

```
Bids:                         Asks:
(empty)                       10001 x [201:50]
```

(Note: there is liquidity on the ask side, but the aggressor is selling, so the relevant opposite side is the bid side, which is empty.)

**Event**: `NewOrder { id=606, side=S, type=MKT, price=0, qty=50, ts=13 }`

**Reports emitted (in order)**:
1. `{ kind=Reject, order_id=606, side=S, price=0, qty=50, ts=13, reason=EmptyBook }`

**Post-event book**:

```
Bids:                         Asks:
(empty)                       10001 x [201:50]
```

**Notes**: a market sell against an empty bid side is rejected, even though there is volume on the ask side. The opposite-side liquidity is what matters for matching, not the same-side liquidity. The book is unchanged.

---

## 5. IOC order semantics

An `IOC` (Immediate or Cancel) order is a limit order with the additional rule that any unfilled residual is cancelled immediately rather than resting on the book. On arrival:

1. The engine always emits an `Acknowledge` first, with the original submitted quantity. (Unlike a market order against an empty book, an IOC is not rejected outright; it acks and then either fills, partially fills with implicit cancel, or fills nothing with implicit cancel.)
2. The order takes liquidity at prices that satisfy its limit, walking the opposite side as in section 2.3.
3. Walking stops when the aggressor's quantity is exhausted, the next maker's price violates the limit, or the opposite side is empty.
4. Any residual is implicitly cancelled (no `Cancel` report). IOC orders never rest.

The "IOC never rests" rule is one of the Phase 1 invariants in section 7.

### 5.1 Worked example IOC-1: IOC fully filling against a single resting order

**Initial book**:

```
Bids:                         Asks:
(empty)                       10001 x [201:30]
```

**Event**: `NewOrder { id=701, side=B, type=IOC, price=10001, qty=30, ts=14 }`

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=701, side=B, price=10001, qty=30, ts=14 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=30, ts=14 }` (maker)
3. `{ kind=Fill, order_id=701, side=B, price=10001, qty=30, ts=14 }` (taker)

**Post-event book**:

```
Bids:                         Asks:
(empty)                       (empty)
```

**Notes**: when an IOC fully fills, its behavior is indistinguishable from a limit order that fully fills as a taker. Submitted=30, filled=30, resting=0, cancelled=0 for order 701.

### 5.2 Worked example IOC-2: IOC partially filling, residual implicitly cancelled

**Initial book**:

```
Bids:                         Asks:
(empty)                       10001 x [201:20]
```

**Event**: `NewOrder { id=702, side=B, type=IOC, price=10001, qty=50, ts=15 }`

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=702, side=B, price=10001, qty=50, ts=15 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=20, ts=15 }` (maker, fully filled)
3. `{ kind=Fill, order_id=702, side=B, price=10001, qty=20, ts=15 }` (taker, partial)

The aggressor has filled 20 of its 50 shares. The opposite side is now empty. The 30-share residual would normally rest at the limit price for a `Limit` order, but for IOC it is implicitly cancelled.

**Post-event book**:

```
Bids:                         Asks:
(empty)                       (empty)
```

**Notes**: IOC's defining feature in this example. Submitted=50, filled=20, resting=0, cancelled=30 for order 702. The conservation law holds: 50 = 20 + 0 + 30.

### 5.3 Worked example IOC-3: IOC against an empty book (acks with zero filled, residual implicitly cancelled)

**Initial book**: empty.

**Event**: `NewOrder { id=703, side=B, type=IOC, price=10001, qty=50, ts=16 }`

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=703, side=B, price=10001, qty=50, ts=16 }`

No fills. The aggressor has 50 shares remaining, all of which are implicitly cancelled. No `Cancel` report is emitted (per the IOC convention: residual is implicitly cancelled).

**Post-event book**: empty (unchanged).

**Notes**: critical corner case. An IOC against an empty book is **not** rejected. It is acknowledged (because submission was valid) and then implicitly cancelled in full. This differs from `Market` against an empty book (section 4.5), which is rejected. Submitted=50, filled=0, resting=0, cancelled=50 for order 703. The conservation law holds: 50 = 0 + 0 + 50.

### 5.4 Worked example IOC-4: IOC at a price that does not cross any resting level (acks with zero filled)

**Initial book**:

```
Bids:                         Asks:
(empty)                       10005 x [201:30]
```

**Event**: `NewOrder { id=704, side=B, type=IOC, price=10001, qty=30, ts=17 }`

The aggressor's limit price is 10001. The best ask is 10005. The aggressor is unwilling to pay more than 10001, so it cannot trade.

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=704, side=B, price=10001, qty=30, ts=17 }`

No fills. The aggressor has 30 shares remaining, all implicitly cancelled.

**Post-event book**:

```
Bids:                         Asks:
(empty)                       10005 x [201:30]
```

**Notes**: another critical corner case. An IOC that cannot cross is acknowledged and then implicitly cancelled in full. The book is unchanged. The aggressor never had a chance to rest because IOC orders never rest. Submitted=30, filled=0, resting=0, cancelled=30 for order 704.

### 5.5 Worked example IOC-5: IOC sweeping two price levels and finishing at limit

**Initial book**:

```
Bids:                         Asks:
(empty)                       10001 x [201:20]
                              10002 x [202:30]
                              10003 x [203:40]
```

**Event**: `NewOrder { id=705, side=B, type=IOC, price=10002, qty=60, ts=18 }`

The aggressor at limit 10002 is willing to take 10001 and 10002 levels, but not 10003.

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=705, side=B, price=10002, qty=60, ts=18 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=20, ts=18 }`
3. `{ kind=Fill, order_id=705, side=B, price=10001, qty=20, ts=18 }`
4. `{ kind=Fill, order_id=202, side=S, price=10002, qty=30, ts=18 }`
5. `{ kind=Fill, order_id=705, side=B, price=10002, qty=30, ts=18 }`

After step 5, the aggressor has filled 50 of 60 shares. Next ask level is 10003, which violates the limit. The 10-share residual is implicitly cancelled.

**Post-event book**:

```
Bids:                         Asks:
(empty)                       10003 x [203:40]
```

**Notes**: IOC against multiple levels. Submitted=60, filled=50, resting=0, cancelled=10 for order 705. Conservation: 60 = 50 + 0 + 10.

### 5.6 Worked example IOC-6: IOC fully filling within a single price level (FIFO honored)

**Initial book**:

```
Bids:                         Asks:
(empty)                       10001 x [201:20, 202:20, 203:20]
```

**Event**: `NewOrder { id=706, side=B, type=IOC, price=10001, qty=40, ts=19 }`

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=706, side=B, price=10001, qty=40, ts=19 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=20, ts=19 }` (front of queue, fully filled)
3. `{ kind=Fill, order_id=706, side=B, price=10001, qty=20, ts=19 }`
4. `{ kind=Fill, order_id=202, side=S, price=10001, qty=20, ts=19 }` (next, fully filled)
5. `{ kind=Fill, order_id=706, side=B, price=10001, qty=20, ts=19 }`

After step 5, the aggressor has filled 40 of 40 shares. It stops because its quantity is exhausted. There is no residual to cancel.

**Post-event book**:

```
Bids:                         Asks:
10000 (empty)                 10001 x [203:20]
```

**Notes**: IOC's interaction with FIFO is the same as Limit's. The IOC takes 201 first, then 202, then stops with quantity exhausted, leaving 203 untouched at the back of the queue. Submitted=40, filled=40, resting=0, cancelled=0 for order 706.

### 5.7 Worked example IOC-7: IOC sell partially filling against multiple bid levels

**Initial book**:

```
Bids:                         Asks:
9999 x [101:30]               (empty)
9998 x [102:20]
9995 x [103:50]
```

**Event**: `NewOrder { id=707, side=S, type=IOC, price=9998, qty=80, ts=20 }`

The aggressor selling at limit 9998 is willing to fill against bids at 9998 and above (i.e., 9999 and 9998). The 9995 level is below the limit.

**Reports emitted (in order)**:
1. `{ kind=Acknowledge, order_id=707, side=S, price=9998, qty=80, ts=20 }`
2. `{ kind=Fill, order_id=101, side=B, price=9999, qty=30, ts=20 }`
3. `{ kind=Fill, order_id=707, side=S, price=9999, qty=30, ts=20 }`
4. `{ kind=Fill, order_id=102, side=B, price=9998, qty=20, ts=20 }`
5. `{ kind=Fill, order_id=707, side=S, price=9998, qty=20, ts=20 }`

After step 5, the aggressor has filled 50 of 80 shares. The next bid level is 9995, which violates the limit (a sell at 9998 will not trade at 9995). The 30-share residual is implicitly cancelled.

**Post-event book**:

```
Bids:                         Asks:
9995 x [103:50]               (empty)
```

**Notes**: confirms IOC sell semantics are symmetric to IOC buy. The 9995 bid is unaffected. Submitted=80, filled=50, resting=0, cancelled=30 for order 707. Conservation: 80 = 50 + 0 + 30.

---

## 6. Cancel-by-id semantics

A `Cancel` event names an `OrderId`. The engine looks the id up in the per-symbol `OrderId -> Order*` map (added to the `Book` in Phase 1 per the companion plan key decision 4).

1. If the lookup hits a resting order, the engine unlinks the order from its `Level`, removes it from the lookup map, removes the now-empty level from the side if applicable, and emits a `Cancel` report carrying the cancelled order's remaining quantity.
2. If the lookup misses (the id was never registered, or it was already fully filled, or it was already cancelled), the engine emits a `Reject` report with `reason = NotFound`. This makes the cancel-by-id path idempotent in the sense that issuing a cancel for an id that is no longer in the book always produces a `Reject` and never a phantom side effect.

The "cancel idempotence" invariant in design spec section 8.2 (item 6) is the property-test version of this rule. It is deferred to a later phase (see section 7 of this document) because Phase 1's invariant set is intentionally minimal.

### 6.1 Worked example CXL-1: cancel a fully resting order

**Initial book**:

```
Bids:                         Asks:
10000 x [101:50, 102:30]      (empty)
```

**Event**: `Cancel { order_id=101, ts=21 }`

**Reports emitted (in order)**:
1. `{ kind=Cancel, order_id=101, side=B, price=10000, qty=50, ts=21 }`

**Post-event book**:

```
Bids:                         Asks:
10000 x [102:30]              (empty)
```

**Notes**: order 101 had 50 shares remaining (it had never traded). The Cancel report carries `qty=50`, which is what was cancelled. Order 102 advances to the front of the level. Submitted=50, filled=0, resting=0, cancelled=50 for order 101.

### 6.2 Worked example CXL-2: cancel a partially filled resting order

**Initial book** (after order 101 partially filled at some prior aggressor):

```
Bids:                         Asks:
10000 x [101:20, 102:30]      (empty)
```

(Order 101 was originally submitted with qty=50; it has been filled for 30 already and has 20 remaining at the front of the level.)

**Event**: `Cancel { order_id=101, ts=22 }`

**Reports emitted (in order)**:
1. `{ kind=Cancel, order_id=101, side=B, price=10000, qty=20, ts=22 }`

**Post-event book**:

```
Bids:                         Asks:
10000 x [102:30]              (empty)
```

**Notes**: the Cancel report carries `qty=20` (the remaining quantity), not `qty=50` (the original submitted quantity). Cumulatively, order 101's lifecycle is: submitted=50, filled=30, resting=0, cancelled=20. Conservation: 50 = 30 + 0 + 20. The engine never re-emits the prior fill information on cancel.

### 6.3 Worked example CXL-3: cancel removes the last order at a level (level is removed)

**Initial book**:

```
Bids:                         Asks:
10000 x [101:50]              (empty)
```

**Event**: `Cancel { order_id=101, ts=23 }`

**Reports emitted (in order)**:
1. `{ kind=Cancel, order_id=101, side=B, price=10000, qty=50, ts=23 }`

**Post-event book**:

```
Bids:                         Asks:
(empty)                       (empty)
```

**Notes**: the level at price 10000 became empty after the cancel and is removed from the side's price map. The next-best-bid query now returns "no bid". This branch is what keeps the `std::map<Price, Level*>` from accumulating empty levels.

### 6.4 Worked example CXL-4: cancel an unknown OrderId (Reject NotFound)

**Initial book**:

```
Bids:                         Asks:
10000 x [101:50]              (empty)
```

**Event**: `Cancel { order_id=999, ts=24 }`

(Order id 999 was never submitted. The lookup map does not contain it.)

**Reports emitted (in order)**:
1. `{ kind=Reject, order_id=999, side=B (sentinel), price=0, qty=0, ts=24, reason=NotFound }`

**Post-event book**:

```
Bids:                         Asks:
10000 x [101:50]              (empty)
```

**Notes**: the engine has no way to know what side, price, or qty an unknown order was. The Reject report carries sentinel zeros for those fields and the reason `NotFound` so the consumer can distinguish this from any other reject. The book is unchanged. (The choice of `Side::Buy` as the sentinel is arbitrary; the C++ engine and Python reference must agree on a single convention. This is the second convention choice flagged for PM review in section 8.)

### 6.5 Worked example CXL-5: cancel an OrderId that was already fully filled (Reject NotFound)

**Initial book**:

```
Bids:                         Asks:
(empty)                       (empty)
```

(Earlier history: order 201 was a sell limit at 10001 for qty=30. A buy aggressor 301 took it in full. Order 201 is no longer in the lookup map.)

**Event**: `Cancel { order_id=201, ts=25 }`

**Reports emitted (in order)**:
1. `{ kind=Reject, order_id=201, side=B (sentinel), price=0, qty=0, ts=25, reason=NotFound }`

**Post-event book**: unchanged.

**Notes**: the cancel-after-fully-filled case is structurally identical to the unknown-id case (section 6.4): the lookup miss is the only signal the engine has. This is the "idempotence" property in disguise: the engine cannot tell whether the order was filled, cancelled, or never existed; it only knows it is not in the book now.

### 6.6 Worked example CXL-6: cancel an OrderId that was already cancelled (Reject NotFound)

**Initial book**:

```
Bids:                         Asks:
10000 x [102:30]              (empty)
```

(Earlier history: order 101 was a buy limit at 10000 for qty=50. It was cancelled at ts=21, per CXL-1. Order 101 is no longer in the lookup map.)

**Event**: `Cancel { order_id=101, ts=26 }`

**Reports emitted (in order)**:
1. `{ kind=Reject, order_id=101, side=B (sentinel), price=0, qty=0, ts=26, reason=NotFound }`

**Post-event book**:

```
Bids:                         Asks:
10000 x [102:30]              (empty)
```

**Notes**: the second cancel of an already-cancelled order is a `Reject` with reason `NotFound`, not a no-op and not a duplicate `Cancel`. This is the cancel-idempotence convention: the engine returns the same kind of report regardless of how many times the same id is cancelled, after the first successful cancel.

---

## 7. Phase 1 invariants (checklist for QA Engineer)

The following five invariants are within scope for Phase 1 and should be turned into property tests by the QA Engineer in Phase 3 (Phase 3 is when `rapidcheck` is wired up; the tests are not written in Phase 1 itself, but the invariants must be established here so the Phase 1 implementation is built against them). Each item below names the design-spec section 8.2 invariant it corresponds to and gives the precise statement.

* [ ] **1. Price-time priority preserved across arbitrary sequences.** For any sequence of `EngineEvent`s applied in order, the order in which orders fill at a given price level is the order in which they were inserted at that level (FIFO at the level). (Design spec 8.2 item 1.)
* [ ] **2. Quantity conservation.** For any `OrderId` ever submitted, `submitted_qty == filled_qty + resting_qty + cancelled_qty` at every point in time after the order has reached a terminal or resting state. (Design spec 8.2 item 2.)
* [ ] **3. No double fill.** No `OrderId` ever receives a stream of `Fill` reports whose cumulative quantity exceeds its submitted quantity. (Design spec 8.2 item 3.)
* [ ] **4. No lost orders.** Every submitted `OrderId` is, at any point in time, in exactly one of these states: `filled` (no remaining qty, all filled), `partially-filled-resting` (some filled, some resting on the book), `fully-resting` (no fills, full qty resting), `cancelled-with-no-fills` (no fills, cancelled), or `cancelled-after-partial-fill` (some fills, then cancelled). The five states are mutually exclusive and collectively exhaustive. (Design spec 8.2 item 4, expanded to make the cancel-after-partial-fill case explicit.)
* [ ] **5. IOC never rests.** No `OrderId` submitted with `OrderType::IOC` is ever found in the book's `OrderId -> Order*` lookup map after the matching loop returns. (Design spec 8.2 item 7.)

The remaining five invariants from design spec section 8.2 are **deferred to later phases**. QA must not write Phase 1 property tests for these.

* [ ] **(Deferred to Phase 4 / Phase 5)** Spread non-negative: `best_ask >= best_bid` at all times between events. The seqlock-protected snapshot lands in Phase 4, and the multi-level interleaving needed to stress this in property tests is more naturally exercised once the seqlock and sampler are in place. (Design spec 8.2 item 5.)
* [ ] **(Deferred to Phase 3 / Phase 7)** Cancel idempotence: a second cancel of the same id returns `Reject reason=NotFound`. The Phase 1 worked examples (CXL-4, CXL-5, CXL-6) cover this in unit tests; the property-test form lands in Phase 3 alongside the other generated-sequence invariants. (Design spec 8.2 item 6.)
* [ ] **(Deferred to Phase 7)** FOK is all-or-nothing. Phase 7 is when `OrderType::FOK` is implemented. (Design spec 8.2 item 8.)
* [ ] **(Deferred to Phase 7)** Post-only never crosses. Phase 7 is when `OrderType::PostOnly` is implemented. (Design spec 8.2 item 9.)
* [ ] **(Deferred to Phase 4)** Top-of-book monotonicity within a tick: between event N and event N+1, top-of-book changes are atomic from a reader's perspective (no torn reads). The seqlock protocol that guarantees this lands in Phase 4. (Design spec 8.2 item 10.)

### 7.1 The conservation law in arithmetic, with a worked example

The conservation law `submitted = filled + resting + cancelled` is the most useful invariant for sanity-checking any worked example in this document. Walk through MKT-3 (section 4.4):

* Order 603 was submitted as a market buy with `qty=100`. So `submitted = 100`.
* It received one Fill at 10001 for 30 shares. So `filled = 30`.
* It is a market order, so it never rests. So `resting = 0`.
* The opposite side emptied with 70 shares remaining. The 70 shares are implicitly cancelled. So `cancelled = 70`.
* Check: `100 == 30 + 0 + 70`. The arithmetic balances.

Walk through IOC-7 (section 5.7):

* Order 707 was submitted as an IOC sell with `qty=80`. So `submitted = 80`.
* It received Fills at 9999 for 30 shares and at 9998 for 20 shares. So `filled = 50`.
* IOC orders never rest. So `resting = 0`.
* The 30-share residual is implicitly cancelled at the limit boundary. So `cancelled = 30`.
* Check: `80 == 50 + 0 + 30`. The arithmetic balances.

Walk through CXL-2 (section 6.2):

* Order 101 was originally submitted as a buy limit with `qty=50`. So `submitted = 50`.
* It was filled for 30 by some prior aggressor. So `filled = 30`.
* The cancel removed the remaining 20 shares from the book. So `cancelled = 20`. After the cancel, `resting = 0`.
* Check: `50 == 30 + 0 + 20`. The arithmetic balances.

The QA Engineer's Phase 3 property test for invariant 2 generates an arbitrary event sequence, replays it through both the C++ engine and the Python reference, and at every intermediate step asserts the conservation law for every `OrderId` that has been submitted so far.

---

## 8. Decisions flagged for PM review

The Quant Domain Validator made two convention choices in this document where the textbook references admit more than one acceptable behavior. These are flagged here so the PM can ratify or override before the Reference Implementation Engineer encodes them in the Python reference.

### 8.1 Market against empty book: single Reject vs Acknowledge-then-Reject

Section 4.1 picks **single `Reject` with `reason=EmptyBook`, no preceding `Acknowledge`**. Some real exchanges emit an `Acknowledge` first. Picking the single-reject form is cleaner for the downstream consumer and matches what a defensible fast path would do (the engine never reached the matching loop in any meaningful sense). The Reference Implementation Engineer must implement this choice; the C++ engine must match. If the PM overrides this to "ack then reject", every MKT-* worked example with an empty opposite side (MKT-4, MKT-7) and the Phase 3 property tests must be updated.

### 8.2 Reject report sentinel side and price for unknown OrderId cancel

Section 6.4 picks `Side::Buy` and `price=0` as sentinels in the `Reject` report when the cancel target `OrderId` is unknown. The engine has no way to know the actual side or price (the order was never registered), so any choice here is arbitrary. `Side::Buy` is the value zero of the `Side` enum, so it is a natural sentinel; a future refactor that adds a third enumerator like `Side::Unknown` would be cleaner but adds a tag bit to the hot path. The Reference Implementation Engineer should match whatever the C++ engine does. If the PM prefers a different sentinel convention, CXL-4, CXL-5, and CXL-6 must be updated.

---

## 9. Citations

The references below are listed with the specific section that supports each rule. Where this document was written without direct access to the cited text, the citation is marked `[citation needed]` so the Citation and Fact Auditor (agent 17) can resolve the exact section number in their pass before this document ships.

* **Harris, Larry. *Trading and Exchanges: Market Microstructure for Practitioners*. Oxford University Press, 2003.**
  * Chapter 6, "Order-Driven Markets": the formal treatment of order precedence rules: price priority is primary; time priority breaks ties at a price level. Cited in section 2.1 and section 2.2 of this document. [citation needed: confirm exact section number within Chapter 6 for the price-time priority statement]
  * Chapter 4 or 5: the canonical taxonomy of order types: limit, market, IOC, FOK, post-only. Cited implicitly in sections 3, 4, 5 of this document. [citation needed: confirm chapter number]
* **O'Hara, Maureen. *Market Microstructure Theory*. Blackwell Publishers, 1995.**
  * Chapter 1, "An Introduction to Market Microstructure": the formal definition of a limit order book as a collection of price levels with FIFO queues at each level. Cited in section 2.1 of this document. [citation needed: confirm exact section number]
* **NASDAQ TotalView-ITCH 5.0 Specification, document version 5.0, NASDAQ MarketWatch.**
  * The `Order Executed Message` (Type `E`) and `Order Executed With Price Message` (Type `C`) document the maker-taker fill semantics that this document's section 2.3 mirrors. [citation needed: confirm exact page or section]
  * The order-priority invariant is implicit in the message protocol: the spec assumes a price-time-priority matching engine. [citation needed: confirm wording]
  * Phase 6 will produce `docs/risk/itch-conformance.md` (a separate document by the same Quant Domain Validator) that lists every ITCH 5.0 message type and its mapping to an `EngineEvent`. The present document references ITCH only for terminology and only where wording is borrowed verbatim.
* **Lyons, Richard K. *The Microstructure Approach to Exchange Rates*. MIT Press, 2001.**
  * Listed as background reading in `agents/01-quant-domain-validator.md` for context on order-book theory. Not directly cited in any rule of this document; left in the references for completeness.

The Citation and Fact Auditor's job in Phase 1 is to resolve every `[citation needed]` above into a specific section, page number, or paragraph reference that another reader could verify by opening the same source.
