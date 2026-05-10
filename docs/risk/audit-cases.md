# Phase 1 Matching Correctness Audit Cases

**Owner**: Risk and Financial Correctness Reviewer (agent 16)
**Phase**: 1 (Core data structures and single-symbol matching)
**Status**: Phase 1 sign-off
**Date**: 2026-05-09

This document is the side-by-side correctness audit of the Phase 1 matching engine. Ten worked examples from `docs/risk/matching-semantics.md` were exercised against both implementations of the matching loop:

* The C++ engine in `src/matching.cpp`, driven by a one-shot capture harness at `tests/integration/audit_capture.cpp`.
* The Python reference in `tests/reference/matching_reference.py`, driven by `tests/reference/run_reference.py` over the same JSON Lines event sequence.

Both implementations were piped through the same JSON Lines wire format described in `tests/reference/run_reference.py`, with object keys in alphabetical order. The two output streams were captured with the harness commands documented in section 12 of this file and compared byte for byte.

The 10 cases were selected per the agent file's Phase 1 task list to span all four order-event types: 3 cases from the limit section (LIM-3, LIM-4, LIM-5), 2 from the market section (MKT-3, MKT-4 with the EmptyBook reject branch), 2 from the IOC section (IOC-2, IOC-4 with the ack-only-no-fills branch), and 3 from the cancel section (CXL-1 success branch, CXL-4 unknown id, CXL-5 already-fully-filled).

The captured streams contain every report the engine emits, including the acknowledges for the seed orders that establish the initial book. The "Reports emitted (in order)" block in `matching-semantics.md` documents only the reports for the aggressor or cancel event under examination. Each case below labels the seed acks (initial book setup) separately from the under-audit reports so the cross-check is direct.

---

## Case 1, LIM-3: aggressive limit fully crossing one resting order

Source: `docs/risk/matching-semantics.md` section 3.3.

### Input event sequence

Verbatim from matching-semantics.md, plus the seed order that establishes the initial book:

* Seed: `NewOrder { id=201, side=S, type=LIM, price=10001, qty=40, ts=2 }`
* Event under audit: `NewOrder { id=301, side=B, type=LIM, price=10001, qty=40, ts=3 }`

### Expected output (matching-semantics.md "Reports emitted")

1. `{ kind=Acknowledge, order_id=301, side=B, price=10001, qty=40, ts=3 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=40, ts=3 }` (maker)
3. `{ kind=Fill, order_id=301, side=B, price=10001, qty=40, ts=3 }` (taker)

### C++ engine actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10001,"qty":40,"reject_reason":"none","side":"sell","ts":2}
{"kind":"ack","order_id":301,"price":10001,"qty":40,"reject_reason":"none","side":"buy","ts":3}
{"kind":"fill","order_id":201,"price":10001,"qty":40,"reject_reason":"none","side":"sell","ts":3}
{"kind":"fill","order_id":301,"price":10001,"qty":40,"reject_reason":"none","side":"buy","ts":3}
```

The first line is the seed ack (resting order 201 entering the book). Lines 2 through 4 are the three reports the worked example documents.

### Python reference actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10001,"qty":40,"reject_reason":"none","side":"sell","ts":2}
{"kind":"ack","order_id":301,"price":10001,"qty":40,"reject_reason":"none","side":"buy","ts":3}
{"kind":"fill","order_id":201,"price":10001,"qty":40,"reject_reason":"none","side":"sell","ts":3}
{"kind":"fill","order_id":301,"price":10001,"qty":40,"reject_reason":"none","side":"buy","ts":3}
```

### Status: PASS

C++ and Python streams are byte-identical. Both match the worked example: ack precedes fills, maker fill precedes taker fill, both fills carry the maker's price (10001) and the aggressor's ts (3).

---

## Case 2, LIM-4: aggressive limit sweeping two price levels with a residual that rests

Source: `docs/risk/matching-semantics.md` section 3.4.

### Input event sequence

* Seed 1: `NewOrder { id=201, side=S, type=LIM, price=10001, qty=30, ts=1 }`
* Seed 2: `NewOrder { id=202, side=S, type=LIM, price=10002, qty=40, ts=2 }`
* Seed 3: `NewOrder { id=203, side=S, type=LIM, price=10003, qty=50, ts=3 }`
* Event under audit: `NewOrder { id=302, side=B, type=LIM, price=10002, qty=100, ts=4 }`

### Expected output (matching-semantics.md "Reports emitted")

1. `{ kind=Acknowledge, order_id=302, side=B, price=10002, qty=100, ts=4 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=30, ts=4 }` (maker, fully filled at 10001)
3. `{ kind=Fill, order_id=302, side=B, price=10001, qty=30, ts=4 }` (taker, partial)
4. `{ kind=Fill, order_id=202, side=S, price=10002, qty=40, ts=4 }` (maker, fully filled at 10002)
5. `{ kind=Fill, order_id=302, side=B, price=10002, qty=40, ts=4 }` (taker, partial)

After the walk, the 30-share residual rests at 10002 on the bid side; level 10003 (order 203 with 50) is untouched.

### C++ engine actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":1}
{"kind":"ack","order_id":202,"price":10002,"qty":40,"reject_reason":"none","side":"sell","ts":2}
{"kind":"ack","order_id":203,"price":10003,"qty":50,"reject_reason":"none","side":"sell","ts":3}
{"kind":"ack","order_id":302,"price":10002,"qty":100,"reject_reason":"none","side":"buy","ts":4}
{"kind":"fill","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":4}
{"kind":"fill","order_id":302,"price":10001,"qty":30,"reject_reason":"none","side":"buy","ts":4}
{"kind":"fill","order_id":202,"price":10002,"qty":40,"reject_reason":"none","side":"sell","ts":4}
{"kind":"fill","order_id":302,"price":10002,"qty":40,"reject_reason":"none","side":"buy","ts":4}
```

Lines 1 through 3 are seed acks (the three resting asks). Lines 4 through 8 are the five reports the worked example documents.

### Python reference actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":1}
{"kind":"ack","order_id":202,"price":10002,"qty":40,"reject_reason":"none","side":"sell","ts":2}
{"kind":"ack","order_id":203,"price":10003,"qty":50,"reject_reason":"none","side":"sell","ts":3}
{"kind":"ack","order_id":302,"price":10002,"qty":100,"reject_reason":"none","side":"buy","ts":4}
{"kind":"fill","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":4}
{"kind":"fill","order_id":302,"price":10001,"qty":30,"reject_reason":"none","side":"buy","ts":4}
{"kind":"fill","order_id":202,"price":10002,"qty":40,"reject_reason":"none","side":"sell","ts":4}
{"kind":"fill","order_id":302,"price":10002,"qty":40,"reject_reason":"none","side":"buy","ts":4}
```

### Status: PASS

C++ and Python streams are byte-identical. Both match the worked example: the aggressor walks 10001 first (fully consumes 30 from order 201), then 10002 (fully consumes 40 from order 202), and stops because the next level (10003) violates its limit. The aggressor's residual of 30 shares would rest at 10002 per LIM-4's "Notes" block; the Phase 1 capture stream records only the report stream, not the post-event book state, but the two engines agree on every emitted report which (combined with the conservation law check) implies they agree on the residual as well.

---

## Case 3, LIM-5: time priority within a level (FIFO is preserved)

Source: `docs/risk/matching-semantics.md` section 3.5.

### Input event sequence

* Seed 1: `NewOrder { id=201, side=S, type=LIM, price=10001, qty=20, ts=1 }`
* Seed 2: `NewOrder { id=202, side=S, type=LIM, price=10001, qty=20, ts=2 }`
* Seed 3: `NewOrder { id=203, side=S, type=LIM, price=10001, qty=20, ts=3 }`
* Event under audit: `NewOrder { id=401, side=B, type=LIM, price=10001, qty=35, ts=5 }`

(All three seeds are at the same price 10001 and pile into a single FIFO queue with 201 at the front.)

### Expected output (matching-semantics.md "Reports emitted")

1. `{ kind=Acknowledge, order_id=401, side=B, price=10001, qty=35, ts=5 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=20, ts=5 }` (front of queue, fully filled)
3. `{ kind=Fill, order_id=401, side=B, price=10001, qty=20, ts=5 }`
4. `{ kind=Fill, order_id=202, side=S, price=10001, qty=15, ts=5 }` (next in queue, partial fill)
5. `{ kind=Fill, order_id=401, side=B, price=10001, qty=15, ts=5 }`

### C++ engine actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10001,"qty":20,"reject_reason":"none","side":"sell","ts":1}
{"kind":"ack","order_id":202,"price":10001,"qty":20,"reject_reason":"none","side":"sell","ts":2}
{"kind":"ack","order_id":203,"price":10001,"qty":20,"reject_reason":"none","side":"sell","ts":3}
{"kind":"ack","order_id":401,"price":10001,"qty":35,"reject_reason":"none","side":"buy","ts":5}
{"kind":"fill","order_id":201,"price":10001,"qty":20,"reject_reason":"none","side":"sell","ts":5}
{"kind":"fill","order_id":401,"price":10001,"qty":20,"reject_reason":"none","side":"buy","ts":5}
{"kind":"fill","order_id":202,"price":10001,"qty":15,"reject_reason":"none","side":"sell","ts":5}
{"kind":"fill","order_id":401,"price":10001,"qty":15,"reject_reason":"none","side":"buy","ts":5}
```

### Python reference actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10001,"qty":20,"reject_reason":"none","side":"sell","ts":1}
{"kind":"ack","order_id":202,"price":10001,"qty":20,"reject_reason":"none","side":"sell","ts":2}
{"kind":"ack","order_id":203,"price":10001,"qty":20,"reject_reason":"none","side":"sell","ts":3}
{"kind":"ack","order_id":401,"price":10001,"qty":35,"reject_reason":"none","side":"buy","ts":5}
{"kind":"fill","order_id":201,"price":10001,"qty":20,"reject_reason":"none","side":"sell","ts":5}
{"kind":"fill","order_id":401,"price":10001,"qty":20,"reject_reason":"none","side":"buy","ts":5}
{"kind":"fill","order_id":202,"price":10001,"qty":15,"reject_reason":"none","side":"sell","ts":5}
{"kind":"fill","order_id":401,"price":10001,"qty":15,"reject_reason":"none","side":"buy","ts":5}
```

### Status: PASS

C++ and Python streams are byte-identical. Both honor FIFO within the price level: order 201 (front) fills first for 20, then order 202 (next) fills for 15, exhausting the aggressor's 35-share quantity. Order 203 (back of queue) is never touched, exactly per the worked example. This is the price-time priority invariant in its purest form (matching-semantics.md section 7 invariant 1).

---

## Case 4, MKT-3: market buy partially filling and the residual implicitly cancelled

Source: `docs/risk/matching-semantics.md` section 4.4.

### Input event sequence

* Seed: `NewOrder { id=201, side=S, type=LIM, price=10001, qty=30, ts=1 }`
* Event under audit: `NewOrder { id=603, side=B, type=MKT, price=0, qty=100, ts=10 }`

The book has only 30 shares of liquidity. The market buy takes them all; the 70-share residual is implicitly cancelled (no Cancel report) per the matching-semantics.md section 4 rule that market orders never rest.

### Expected output (matching-semantics.md "Reports emitted")

1. `{ kind=Acknowledge, order_id=603, side=B, price=0, qty=100, ts=10 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=30, ts=10 }`
3. `{ kind=Fill, order_id=603, side=B, price=10001, qty=30, ts=10 }`

(After step 3, the opposite side empties and the 70-share residual is implicitly cancelled. No further report.)

### C++ engine actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":1}
{"kind":"ack","order_id":603,"price":0,"qty":100,"reject_reason":"none","side":"buy","ts":10}
{"kind":"fill","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":10}
{"kind":"fill","order_id":603,"price":10001,"qty":30,"reject_reason":"none","side":"buy","ts":10}
```

### Python reference actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":1}
{"kind":"ack","order_id":603,"price":0,"qty":100,"reject_reason":"none","side":"buy","ts":10}
{"kind":"fill","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":10}
{"kind":"fill","order_id":603,"price":10001,"qty":30,"reject_reason":"none","side":"buy","ts":10}
```

### Status: PASS

C++ and Python streams are byte-identical. Both match the worked example: the market order's ack carries `price=0` (the sentinel for unused price field), `qty=100` (submitted qty, not filled qty), and the maker fill plus taker fill both carry the maker's price (10001) and 30 shares each. No fourth report appears for the 70-share residual: the residual is implicitly cancelled exactly as the worked example mandates. Conservation law check: 100 = 30 filled + 0 resting + 70 cancelled (matches matching-semantics.md section 7.1 worked walkthrough).

---

## Case 5, MKT-4: market buy against an empty book is rejected

Source: `docs/risk/matching-semantics.md` section 4.5. This is the EmptyBook reject branch required by the brief.

### Input event sequence

Initial book: empty.

* Event under audit: `NewOrder { id=604, side=B, type=MKT, price=0, qty=50, ts=11 }`

### Expected output (matching-semantics.md "Reports emitted")

1. `{ kind=Reject, order_id=604, side=B, price=0, qty=50, ts=11, reason=EmptyBook }`

Per the section 4.1 convention, no `Acknowledge` precedes the `Reject`. The `qty` carried on the `Reject` is the original submitted quantity (50).

### C++ engine actual output (JSON Lines)

```
{"kind":"reject","order_id":604,"price":0,"qty":50,"reject_reason":"empty_book","side":"buy","ts":11}
```

### Python reference actual output (JSON Lines)

```
{"kind":"reject","order_id":604,"price":0,"qty":50,"reject_reason":"empty_book","side":"buy","ts":11}
```

### Status: PASS

C++ and Python streams are byte-identical. Both implementations honor the section 4.1 convention choice: a single `Reject` with `reason=empty_book`, no preceding `Acknowledge`. The submitted quantity (50) is preserved on the Reject so a downstream consumer can see exactly what was rejected. The `price=0` sentinel matches the matching-semantics.md notation for market orders.

---

## Case 6, IOC-2: IOC partially filling, residual implicitly cancelled

Source: `docs/risk/matching-semantics.md` section 5.2.

### Input event sequence

* Seed: `NewOrder { id=201, side=S, type=LIM, price=10001, qty=20, ts=1 }`
* Event under audit: `NewOrder { id=702, side=B, type=IOC, price=10001, qty=50, ts=15 }`

The aggressor has 50 shares; the book has only 20. The IOC takes the 20, then implicitly cancels the 30-share residual (section 5 rule 4: IOC orders never rest).

### Expected output (matching-semantics.md "Reports emitted")

1. `{ kind=Acknowledge, order_id=702, side=B, price=10001, qty=50, ts=15 }`
2. `{ kind=Fill, order_id=201, side=S, price=10001, qty=20, ts=15 }` (maker, fully filled)
3. `{ kind=Fill, order_id=702, side=B, price=10001, qty=20, ts=15 }` (taker, partial)

### C++ engine actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10001,"qty":20,"reject_reason":"none","side":"sell","ts":1}
{"kind":"ack","order_id":702,"price":10001,"qty":50,"reject_reason":"none","side":"buy","ts":15}
{"kind":"fill","order_id":201,"price":10001,"qty":20,"reject_reason":"none","side":"sell","ts":15}
{"kind":"fill","order_id":702,"price":10001,"qty":20,"reject_reason":"none","side":"buy","ts":15}
```

### Python reference actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10001,"qty":20,"reject_reason":"none","side":"sell","ts":1}
{"kind":"ack","order_id":702,"price":10001,"qty":50,"reject_reason":"none","side":"buy","ts":15}
{"kind":"fill","order_id":201,"price":10001,"qty":20,"reject_reason":"none","side":"sell","ts":15}
{"kind":"fill","order_id":702,"price":10001,"qty":20,"reject_reason":"none","side":"buy","ts":15}
```

### Status: PASS

C++ and Python streams are byte-identical. Both ack the IOC with the submitted quantity (50), fill 20 shares, and emit no further report for the 30-share residual: the residual is implicitly cancelled, exactly as the worked example mandates. Conservation: 50 = 20 filled + 0 resting + 30 cancelled.

---

## Case 7, IOC-4: IOC at a price that does not cross any resting level (ack-only-no-fills)

Source: `docs/risk/matching-semantics.md` section 5.4. This is the ack-only-no-fills branch required by the brief.

### Input event sequence

* Seed: `NewOrder { id=201, side=S, type=LIM, price=10005, qty=30, ts=1 }`
* Event under audit: `NewOrder { id=704, side=B, type=IOC, price=10001, qty=30, ts=17 }`

The aggressor's limit price is 10001; the best ask is 10005. The aggressor is unwilling to pay more than 10001, so it cannot trade. Per the matching-semantics.md "Notes" block: "An IOC that cannot cross is acknowledged and then implicitly cancelled in full. The book is unchanged."

### Expected output (matching-semantics.md "Reports emitted")

1. `{ kind=Acknowledge, order_id=704, side=B, price=10001, qty=30, ts=17 }`

(No fills; the 30-share residual is implicitly cancelled.)

### C++ engine actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10005,"qty":30,"reject_reason":"none","side":"sell","ts":1}
{"kind":"ack","order_id":704,"price":10001,"qty":30,"reject_reason":"none","side":"buy","ts":17}
```

### Python reference actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10005,"qty":30,"reject_reason":"none","side":"sell","ts":1}
{"kind":"ack","order_id":704,"price":10001,"qty":30,"reject_reason":"none","side":"buy","ts":17}
```

### Status: PASS

C++ and Python streams are byte-identical. Both implementations correctly distinguish IOC from market: an IOC that cannot cross is acknowledged, not rejected. (Compare MKT-4 above, where the market order against an empty book was a single Reject.) The 30-share residual is implicitly cancelled with no further report. The seed sell at 10005 is unchanged in both cases.

---

## Case 8, CXL-1: cancel a fully resting order (success branch)

Source: `docs/risk/matching-semantics.md` section 6.1.

### Input event sequence

* Seed 1: `NewOrder { id=101, side=B, type=LIM, price=10000, qty=50, ts=1 }`
* Seed 2: `NewOrder { id=102, side=B, type=LIM, price=10000, qty=30, ts=2 }`
* Event under audit: `Cancel { order_id=101, ts=21 }`

### Expected output (matching-semantics.md "Reports emitted")

1. `{ kind=Cancel, order_id=101, side=B, price=10000, qty=50, ts=21 }`

### C++ engine actual output (JSON Lines)

```
{"kind":"ack","order_id":101,"price":10000,"qty":50,"reject_reason":"none","side":"buy","ts":1}
{"kind":"ack","order_id":102,"price":10000,"qty":30,"reject_reason":"none","side":"buy","ts":2}
{"kind":"cancel","order_id":101,"price":10000,"qty":50,"reject_reason":"none","side":"buy","ts":21}
```

### Python reference actual output (JSON Lines)

```
{"kind":"ack","order_id":101,"price":10000,"qty":50,"reject_reason":"none","side":"buy","ts":1}
{"kind":"ack","order_id":102,"price":10000,"qty":30,"reject_reason":"none","side":"buy","ts":2}
{"kind":"cancel","order_id":101,"price":10000,"qty":50,"reject_reason":"none","side":"buy","ts":21}
```

### Status: PASS

C++ and Python streams are byte-identical. Both implementations honor the section 6.1 success-branch contract: the Cancel report carries the cancelled order's resting `side` (buy), `price` (10000), `qty` (50), and the cancel event's `ts` (21). Order 102 remains unaffected at the back of the now-shortened queue, consistent with the post-event book in the worked example.

---

## Case 9, CXL-4: cancel an unknown OrderId (Reject NotFound)

Source: `docs/risk/matching-semantics.md` section 6.4. This is one of the two NotFound branches required by the brief.

### Input event sequence

* Seed: `NewOrder { id=101, side=B, type=LIM, price=10000, qty=50, ts=1 }`
* Event under audit: `Cancel { order_id=999, ts=24 }`

(Order id 999 was never registered with the engine. The lookup map does not contain it.)

### Expected output (matching-semantics.md "Reports emitted")

1. `{ kind=Reject, order_id=999, side=B (sentinel), price=0, qty=0, ts=24, reason=NotFound }`

Per section 8.2 convention, the sentinel side for a Reject NotFound is `Buy` (the zero value of the enum) and the sentinel price and qty are 0.

### C++ engine actual output (JSON Lines)

```
{"kind":"ack","order_id":101,"price":10000,"qty":50,"reject_reason":"none","side":"buy","ts":1}
{"kind":"reject","order_id":999,"price":0,"qty":0,"reject_reason":"not_found","side":"buy","ts":24}
```

### Python reference actual output (JSON Lines)

```
{"kind":"ack","order_id":101,"price":10000,"qty":50,"reject_reason":"none","side":"buy","ts":1}
{"kind":"reject","order_id":999,"price":0,"qty":0,"reject_reason":"not_found","side":"buy","ts":24}
```

### Status: PASS

C++ and Python streams are byte-identical. Both implementations honor the section 8.2 sentinel convention: `side="buy"`, `price=0`, `qty=0`, `reject_reason="not_found"`. The book is unchanged: the seed's ack appears in both streams and order 101 remains in the book. The engine has no way to know the unknown id's side, price, or qty, so the sentinels are the only honest answer.

---

## Case 10, CXL-5: cancel an OrderId that was already fully filled (Reject NotFound)

Source: `docs/risk/matching-semantics.md` section 6.5. This is the second NotFound branch required by the brief: cancel-after-fully-filled is structurally identical to cancel-of-unknown.

### Input event sequence

* Seed 1: `NewOrder { id=201, side=S, type=LIM, price=10001, qty=30, ts=1 }`
* Cross: `NewOrder { id=301, side=B, type=LIM, price=10001, qty=30, ts=2 }` (fully consumes order 201)
* Event under audit: `Cancel { order_id=201, ts=25 }`

By the time the cancel arrives, order 201 has been fully filled and is no longer in the lookup map.

### Expected output (matching-semantics.md "Reports emitted")

1. `{ kind=Reject, order_id=201, side=B (sentinel), price=0, qty=0, ts=25, reason=NotFound }`

### C++ engine actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":1}
{"kind":"ack","order_id":301,"price":10001,"qty":30,"reject_reason":"none","side":"buy","ts":2}
{"kind":"fill","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":2}
{"kind":"fill","order_id":301,"price":10001,"qty":30,"reject_reason":"none","side":"buy","ts":2}
{"kind":"reject","order_id":201,"price":0,"qty":0,"reject_reason":"not_found","side":"buy","ts":25}
```

### Python reference actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":1}
{"kind":"ack","order_id":301,"price":10001,"qty":30,"reject_reason":"none","side":"buy","ts":2}
{"kind":"fill","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":2}
{"kind":"fill","order_id":301,"price":10001,"qty":30,"reject_reason":"none","side":"buy","ts":2}
{"kind":"reject","order_id":201,"price":0,"qty":0,"reject_reason":"not_found","side":"buy","ts":25}
```

### Status: PASS

C++ and Python streams are byte-identical. Both implementations correctly produce the cancel-after-fully-filled Reject NotFound. The cancel-after-fully-filled case is structurally identical to the cancel-of-unknown case (CXL-4): the lookup miss is the only signal the engine has. Both implementations emit the same Reject report regardless of why the id is missing, satisfying the cancel-idempotence convention from matching-semantics.md section 6.

---

## 11. Summary table

| # | Case  | Branch under audit                         | C++ vs expected | Python vs expected | C++ vs Python | Status |
|---|-------|--------------------------------------------|-----------------|--------------------|----------------|--------|
| 1 | LIM-3 | Aggressive limit fully crossing one resting | match | match | byte-identical | PASS |
| 2 | LIM-4 | Multi-level sweep with residual that rests  | match | match | byte-identical | PASS |
| 3 | LIM-5 | FIFO time priority within a level           | match | match | byte-identical | PASS |
| 4 | MKT-3 | Market partial fill, residual implicit cancel | match | match | byte-identical | PASS |
| 5 | MKT-4 | Market vs empty book, single Reject EmptyBook | match | match | byte-identical | PASS |
| 6 | IOC-2 | IOC partial fill, residual implicit cancel  | match | match | byte-identical | PASS |
| 7 | IOC-4 | IOC at non-crossing price, ack-only-no-fills | match | match | byte-identical | PASS |
| 8 | CXL-1 | Cancel resting order, success branch        | match | match | byte-identical | PASS |
| 9 | CXL-4 | Cancel unknown id, Reject NotFound          | match | match | byte-identical | PASS |
|10 | CXL-5 | Cancel after fully filled, Reject NotFound  | match | match | byte-identical | PASS |

All 10 cases pass in both implementations. Both implementations also agree byte for byte across the full output stream.

---

## 12. How to reproduce this audit

The captures above were produced from the project root with these commands. The procedure is deterministic; rerunning produces byte-identical output (see `tests/reference/test_reference.py::DeterminismTests` for the determinism contract on the reference side, and the integration test's `ProducesIdenticalReports` for the C++ side).

### 12.1 Build the audit capture binary

The C++ capture harness lives at `tests/integration/audit_capture.cpp`. It is a one-shot tool, not a unit test, and is intentionally not wired into ctest so the QA Engineer's parameterized integration test corpus stays the canonical CI gate. Build it directly against the existing `libmeridian.a`:

```
g++ -std=c++20 -O2 -Iinclude tests/integration/audit_capture.cpp \
    build/src/libmeridian.a -o build/bin/audit_capture
```

(`build/src/libmeridian.a` is produced by the standard `cmake --build build` at the project root with default options.)

### 12.2 Capture the C++ engine output

```
build/bin/audit_capture
```

Each `=== <CASE> ===` block prints the engine's full JSON Lines stream for that case (seed acks plus the under-audit reports).

### 12.3 Capture the Python reference output

The 10 cases as JSON Lines events are run through `tests/reference/run_reference.py`, which is the same script the integration test uses to drive the reference. One process per case so each case starts with a clean engine instance:

```
PYTHONPATH=tests/reference python3 tests/reference/run_reference.py < <input.jsonl>
```

The exact event streams for each case are documented above under each case's "Input event sequence" section, in JSON Lines form when re-encoded into the wire format from `run_reference.py`'s docstring.

### 12.4 Cross-check

```
diff <cpp.out> <py.out>
```

Empty diff means byte-for-byte agreement. The capture run that produced this document had a clean diff across all 10 cases combined.

### 12.5 Independent regression checks already in CI

* `tests/reference/test_reference.py` covers every worked example in `matching-semantics.md`, including all 26 cases (LIM-1 to LIM-7, MKT-1 to MKT-6, IOC-1 to IOC-7, CXL-1 to CXL-6) plus conservation-law walkthroughs and a determinism test.
* `tests/integration/test_engine_vs_reference.cpp` runs 15 hand-crafted scenarios through both engines and asserts byte-identical JSON output (CI gate). The QA Engineer's parallel Phase 1 dispatch is expanding this to a 60-scenario corpus.

This audit covers a subset of those tests, picked per the agent file Phase 1 task list to span all four order-event types and both NotFound branches. The CI gates are the production source of truth; this document is the human-readable side-by-side correctness audit for the PM and the Citation and Fact Auditor.

---

## 13. Sign-off

**Phase 1 matching correctness signed off.** All 10 audit cases pass in both implementations, and the C++ engine and Python reference produce byte-identical output for every case. No divergences blocking sign-off.

The Reference Implementation Engineer's Python reference and the Engine Developer's C++ engine agree on:

* Acknowledge precedes every accepted new order's fills (sections 2.3, 3, 5).
* Maker fill precedes taker fill at every match (section 1 notation).
* Trade price is the maker's price; trade timestamp is the aggressor's `ts` (section 1).
* Market against an empty book is a single Reject with `reason=EmptyBook`, no preceding Acknowledge (section 4.1).
* IOC against an empty book or a non-crossing price is acknowledged and then implicitly cancelled with no further report (section 5).
* Market and IOC residuals are implicitly cancelled with no `Cancel` report (sections 4, 5).
* Cancel of a resting order is a `Cancel` report carrying the cancelled order's resting `side`, `price`, and remaining `qty` (section 6).
* Cancel of a missing id (unknown, fully filled, or already cancelled) is a Reject with sentinel `side=buy`, `price=0`, `qty=0`, `reason=NotFound` (section 6, section 8.2 convention).

Open follow-up items for later phases, recorded for context but not blocking Phase 1:

* The remaining 16 worked examples in `matching-semantics.md` (LIM-1, LIM-2, LIM-6, LIM-7, MKT-1, MKT-2, MKT-5, MKT-6, IOC-1, IOC-3, IOC-5, IOC-6, IOC-7, CXL-2, CXL-3, CXL-6) are covered by `tests/reference/test_reference.py` and (in part) by `tests/integration/test_engine_vs_reference.cpp`. The Risk Reviewer's per-phase audit obligation is to spot-check, not to copy every test into this document; the integration test file is the comprehensive corpus and the QA Engineer's parallel dispatch is expanding it to ~60 scenarios.
* Phase 2 will extend this document with cancel-after-partial-fill (the remainder branch) and cross-symbol cancel cases (per agent file Phase 2 tasks).
* Phase 3 will add hand-walkthroughs of generated rapidcheck cases for the five Phase 1 invariants (per agent file Phase 3 tasks).

Reviewer: Risk and Financial Correctness Reviewer (agent 16), 2026-05-09.
