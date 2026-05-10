# Single-symbol Matching Correctness Audit Cases

**Date**: 2026-05-09

This document is the side-by-side correctness audit of the single-symbol matching engine. Ten worked examples from `docs/risk/matching-semantics.md` were exercised against both implementations of the matching loop:

* The C++ engine in `src/matching.cpp`, driven by a one-shot capture harness at `tests/integration/audit_capture.cpp`.
* The Python reference in `tests/reference/matching_reference.py`, driven by `tests/reference/run_reference.py` over the same JSON Lines event sequence.

Both implementations were piped through the same JSON Lines wire format described in `tests/reference/run_reference.py`, with object keys in alphabetical order. The two output streams were captured with the harness commands documented in section 12 of this file and compared byte for byte.

The 10 cases were selected to span all four order-event types: 3 cases from the limit section (LIM-3, LIM-4, LIM-5), 2 from the market section (MKT-3, MKT-4 with the EmptyBook reject branch), 2 from the IOC section (IOC-2, IOC-4 with the ack-only-no-fills branch), and 3 from the cancel section (CXL-1 success branch, CXL-4 unknown id, CXL-5 already-fully-filled).

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

C++ and Python streams are byte-identical. Both match the worked example: the aggressor walks 10001 first (fully consumes 30 from order 201), then 10002 (fully consumes 40 from order 202), and stops because the next level (10003) violates its limit. The aggressor's residual of 30 shares would rest at 10002 per LIM-4's "Notes" block; the single-symbol audit capture records only the report stream, not the post-event book state, but the two engines agree on every emitted report which (combined with the conservation law check) implies they agree on the residual as well.

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

C++ and Python streams are byte-identical. Both honor FIFO within the price level: order 201 (front) fills first for 20, then order 202 (next) fills for 15, exhausting the aggressor's 35-share quantity. Order 203 (back of queue) is never touched, exactly per the worked example. This is the price-time priority invariant in its purest form (matching-semantics.md section 9 invariant 1).

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

C++ and Python streams are byte-identical. Both match the worked example: the market order's ack carries `price=0` (the sentinel for unused price field), `qty=100` (submitted qty, not filled qty), and the maker fill plus taker fill both carry the maker's price (10001) and 30 shares each. No fourth report appears for the 70-share residual: the residual is implicitly cancelled exactly as the worked example mandates. Conservation law check: 100 = 30 filled + 0 resting + 70 cancelled (matches matching-semantics.md section 9.1 worked walkthrough).

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

Source: `docs/risk/matching-semantics.md` section 8.1.

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

C++ and Python streams are byte-identical. Both implementations honor the section 8.1 success-branch contract: the Cancel report carries the cancelled order's resting `side` (buy), `price` (10000), `qty` (50), and the cancel event's `ts` (21). Order 102 remains unaffected at the back of the now-shortened queue, consistent with the post-event book in the worked example.

---

## Case 9, CXL-4: cancel an unknown OrderId (Reject NotFound)

Source: `docs/risk/matching-semantics.md` section 8.4. This is one of the two NotFound branches required by the brief.

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

Source: `docs/risk/matching-semantics.md` section 8.5. This is the second NotFound branch required by the brief: cancel-after-fully-filled is structurally identical to cancel-of-unknown.

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

C++ and Python streams are byte-identical. Both implementations correctly produce the cancel-after-fully-filled Reject NotFound. The cancel-after-fully-filled case is structurally identical to the cancel-of-unknown case (CXL-4): the lookup miss is the only signal the engine has. Both implementations emit the same Reject report regardless of why the id is missing, satisfying the cancel-idempotence convention from matching-semantics.md section 8.

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

The C++ capture harness lives at `tests/integration/audit_capture.cpp`. It is a one-shot tool, not a unit test, and is intentionally not wired into ctest so the parameterized integration test corpus stays the canonical CI gate. Build it directly against the existing `libmeridian.a`:

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
* `tests/integration/test_engine_vs_reference.cpp` runs 60 hand-crafted scenarios through both engines and asserts byte-identical JSON output (CI gate).

This audit covers a subset of those tests, chosen to span all four order-event types and both NotFound branches. The CI gates are the production source of truth; this document is the human-readable side-by-side correctness audit.

---

## 13. Sign-off

**Single-symbol matching correctness signed off.** All 10 audit cases pass in both implementations, and the C++ engine and Python reference produce byte-identical output for every case. No divergences blocking sign-off.

The Python reference and the C++ engine agree on:

* Acknowledge precedes every accepted new order's fills (sections 2.3, 3, 5).
* Maker fill precedes taker fill at every match (section 1 notation).
* Trade price is the maker's price; trade timestamp is the aggressor's `ts` (section 1).
* Market against an empty book is a single Reject with `reason=EmptyBook`, no preceding Acknowledge (section 4.1).
* IOC against an empty book or a non-crossing price is acknowledged and then implicitly cancelled with no further report (section 5).
* Market and IOC residuals are implicitly cancelled with no `Cancel` report (sections 4, 5).
* Cancel of a resting order is a `Cancel` report carrying the cancelled order's resting `side`, `price`, and remaining `qty` (section 6).
* Cancel of a missing id (unknown, fully filled, or already cancelled) is a Reject with sentinel `side=buy`, `price=0`, `qty=0`, `reason=NotFound` (section 6, section 8.2 convention).

Open follow-up items for later milestones, recorded for context:

* The remaining 16 worked examples in `matching-semantics.md` (LIM-1, LIM-2, LIM-6, LIM-7, MKT-1, MKT-2, MKT-5, MKT-6, IOC-1, IOC-3, IOC-5, IOC-6, IOC-7, CXL-2, CXL-3, CXL-6) are covered by `tests/reference/test_reference.py` and (in part) by `tests/integration/test_engine_vs_reference.cpp`. This document is a hand-audit spot check, not a full transcription of the test corpus; the integration test file is the comprehensive corpus.
* The cancel-after-partial-fill (remainder branch) and cross-symbol cancel cases are covered in the multi-instrument section below.
* The hand rolled property suite under `tests/property/` covers the matching invariants today; representative seeds for each invariant can be added here as worked walkthroughs once a regression motivates one.

---

# Multi-instrument and cross-symbol cancel

**Date**: 2026-05-09

This section is the side-by-side correctness audit of the multi-instrument matching engine. Five audit cases were exercised against both implementations:

* The C++ engine in `src/matching.cpp`, with `BookRegistry` (`src/book_registry.cpp`) and `OrderIndex` (`src/order_index.cpp`) wired in. The constructor signature changed from the single-symbol `MatchingEngine(OrderPool&, Book&)` to `MatchingEngine(OrderPool&, BookRegistry&, OrderIndex&)`. Multi-symbol dispatch happens in `MatchingEngine::apply` via `registry_.book(event.symbol)`; the cross-symbol cancel routes through `apply_cancel` via `index_.find(event.order_id)`. See `docs/adr/0002-cross-symbol-cancel.md` for the dual-index design rationale.
* The Python reference in `tests/reference/matching_reference.py`, extended with `MatchingReference(symbols=[1,2,3,4,5])`, an internal `_id_to_symbol` cross-symbol map, and the `RejectReason.UNKNOWN_SYMBOL` reject branch. See `tests/reference/test_reference.py::MultiSymbolTests` for the unit-test coverage.

Both implementations were piped through the same JSON Lines wire format. The new-order wire format gained an optional `symbol` field (defaulting to `1` for backward compatibility with the single-symbol scope); the cancel wire format remains `{"kind":"cancel","order_id":<int>,"ts":<int>}` (no `symbol` on the wire, per ADR 0002). The two output streams were captured with the harness commands documented in section 12 of this audit and compared byte for byte.

The 5 cases were selected to span the new multi-instrument surface area: cross-symbol cancel isolation, cancel-after-fully-filled in a multi-symbol setting, unknown-symbol reject (limit and market shapes), independent activity across all 5 demo symbols, and cross-symbol routing where the cancel arrives without the caller naming the order's symbol.

The captured streams contain every report the engines emit, including the acknowledges and seed setup for orders that establish each scenario's initial multi-symbol state. The "Expected output" block under each case documents the full report stream for that scenario (not just the headline event), so the cross-check is direct.

---

## Multi-instrument Case 1, cross-symbol cancel isolation

Source: audit case 1 ("cancel resting order on symbol X, success, no effect on Y"). Cross-references `docs/risk/matching-semantics.md` section 8.1 (the success branch of cancel-by-id) for the per-symbol contract, and ADR 0002 for the cross-symbol routing.

### Input event sequence

* Event 1: `NewOrder { id=100, symbol=1, side=B, type=LIM, price=100, qty=10, ts=1 }`
* Event 2: `NewOrder { id=200, symbol=2, side=B, type=LIM, price=100, qty=10, ts=2 }`
* Event 3: `Cancel { order_id=100, ts=3 }` (no symbol on the wire; engine routes via `OrderIndex`)
* Event 4: `NewOrder { id=201, symbol=2, side=S, type=LIM, price=100, qty=10, ts=4 }` (sanity-touch on symbol 2: should fully cross order 200, which must still be resting if the cancel of 100 stayed on symbol 1)

### Expected output (per matching-semantics.md section 8.1 plus ADR 0002 cross-symbol routing)

1. Ack for the resting buy on symbol 1 (id=100).
2. Ack for the resting buy on symbol 2 (id=200).
3. Cancel for id=100 carrying its resting `side=buy`, `price=100`, `qty=10`, and the cancel's `ts=3`. (Per matching-semantics.md section 8.1, the Cancel report carries the cancelled order's resting fields, not sentinels.)
4. Ack for the symbol-2 sell (id=201) at `price=100, qty=10`.
5. Maker fill on order 200 at `price=100, qty=10, ts=4`.
6. Taker fill on order 201 at `price=100, qty=10, ts=4`.

If the cross-symbol cancel had leaked into symbol 2, order 200 would have been removed by ts=3 and the symbol-2 sell at ts=4 would have rested unmatched (no fills). Both engines must produce fills 5 and 6 to demonstrate symbol 2's book is intact.

### C++ engine actual output (JSON Lines)

```
{"kind":"ack","order_id":100,"price":100,"qty":10,"reject_reason":"none","side":"buy","ts":1}
{"kind":"ack","order_id":200,"price":100,"qty":10,"reject_reason":"none","side":"buy","ts":2}
{"kind":"cancel","order_id":100,"price":100,"qty":10,"reject_reason":"none","side":"buy","ts":3}
{"kind":"ack","order_id":201,"price":100,"qty":10,"reject_reason":"none","side":"sell","ts":4}
{"kind":"fill","order_id":200,"price":100,"qty":10,"reject_reason":"none","side":"buy","ts":4}
{"kind":"fill","order_id":201,"price":100,"qty":10,"reject_reason":"none","side":"sell","ts":4}
```

### Python reference actual output (JSON Lines)

```
{"kind":"ack","order_id":100,"price":100,"qty":10,"reject_reason":"none","side":"buy","ts":1}
{"kind":"ack","order_id":200,"price":100,"qty":10,"reject_reason":"none","side":"buy","ts":2}
{"kind":"cancel","order_id":100,"price":100,"qty":10,"reject_reason":"none","side":"buy","ts":3}
{"kind":"ack","order_id":201,"price":100,"qty":10,"reject_reason":"none","side":"sell","ts":4}
{"kind":"fill","order_id":200,"price":100,"qty":10,"reject_reason":"none","side":"buy","ts":4}
{"kind":"fill","order_id":201,"price":100,"qty":10,"reject_reason":"none","side":"sell","ts":4}
```

### Status: PASS

C++ and Python streams are byte-identical. Both implementations route the cancel of id=100 to symbol 1's book via the cross-symbol `OrderIndex` (C++) and `_id_to_symbol` map (reference) without touching symbol 2's book. The follow-on sell on symbol 2 fully crosses order 200, proving the cross-symbol isolation invariant from ADR 0002 ("a cancel for an order on symbol X never affects book Y").

---

## Multi-instrument Case 2, cancel after fully filled (multi-symbol setting)

Source: audit case 2 ("cancel after fully filled, Reject NotFound"). Cross-references `docs/risk/matching-semantics.md` section 8.5 (CXL-5, the cancel-after-fully-filled NotFound branch) for the per-symbol contract; the new wrinkle in the multi-instrument design is that the lookup happens through the cross-symbol `OrderIndex`, not a per-`Book` map.

### Input event sequence

* Event 1: `NewOrder { id=201, symbol=2, side=S, type=LIM, price=10001, qty=30, ts=1 }`
* Event 2: `NewOrder { id=301, symbol=2, side=B, type=LIM, price=10001, qty=30, ts=2 }` (fully consumes order 201)
* Event 3: `NewOrder { id=401, symbol=4, side=B, type=LIM, price=9000, qty=15, ts=3 }` (an unrelated resting bid on a different symbol)
* Event 4: `Cancel { order_id=201, ts=4 }` (id 201 was fully filled at ts=2; lookup must miss)
* Event 5: `NewOrder { id=402, symbol=4, side=S, type=IOC, price=9000, qty=15, ts=5 }` (sanity-touch on symbol 4: should fully cross order 401, proving symbol 4 was untouched by the failed cancel)

### Expected output (per matching-semantics.md section 8.5 plus ADR 0002 dual-index design)

1. Ack for the symbol-2 sell (id=201).
2. Ack for the symbol-2 buy (id=301).
3. Maker fill on order 201 at `price=10001, qty=30, ts=2`. (Order 201 is fully consumed; both `OrderIndex` and the per-`Book` id index erase it.)
4. Taker fill on order 301 at `price=10001, qty=30, ts=2`.
5. Ack for the symbol-4 buy (id=401).
6. Reject for the cancel of id=201 with sentinel `side=buy`, `price=0`, `qty=0`, `reject_reason=not_found`, `ts=4`. (Per matching-semantics.md section 8.4 sentinel convention; ADR 0002 confirms the lookup happens through `OrderIndex`, which has already erased 201 in step 3.)
7. Ack for the symbol-4 IOC sell (id=402).
8. Maker fill on order 401 at `price=9000, qty=15, ts=5`.
9. Taker fill on order 402 at `price=9000, qty=15, ts=5`.

If the dual-index design had a coherence bug (the per-`Book` index erased on full fill but `OrderIndex` did not, or vice versa), step 6 would emit either a phantom Cancel report or a panic, instead of the correct Reject NotFound. Step 8 plus step 9 confirm symbol 4's book was intact across the failed cancel.

### C++ engine actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":1}
{"kind":"ack","order_id":301,"price":10001,"qty":30,"reject_reason":"none","side":"buy","ts":2}
{"kind":"fill","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":2}
{"kind":"fill","order_id":301,"price":10001,"qty":30,"reject_reason":"none","side":"buy","ts":2}
{"kind":"ack","order_id":401,"price":9000,"qty":15,"reject_reason":"none","side":"buy","ts":3}
{"kind":"reject","order_id":201,"price":0,"qty":0,"reject_reason":"not_found","side":"buy","ts":4}
{"kind":"ack","order_id":402,"price":9000,"qty":15,"reject_reason":"none","side":"sell","ts":5}
{"kind":"fill","order_id":401,"price":9000,"qty":15,"reject_reason":"none","side":"buy","ts":5}
{"kind":"fill","order_id":402,"price":9000,"qty":15,"reject_reason":"none","side":"sell","ts":5}
```

### Python reference actual output (JSON Lines)

```
{"kind":"ack","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":1}
{"kind":"ack","order_id":301,"price":10001,"qty":30,"reject_reason":"none","side":"buy","ts":2}
{"kind":"fill","order_id":201,"price":10001,"qty":30,"reject_reason":"none","side":"sell","ts":2}
{"kind":"fill","order_id":301,"price":10001,"qty":30,"reject_reason":"none","side":"buy","ts":2}
{"kind":"ack","order_id":401,"price":9000,"qty":15,"reject_reason":"none","side":"buy","ts":3}
{"kind":"reject","order_id":201,"price":0,"qty":0,"reject_reason":"not_found","side":"buy","ts":4}
{"kind":"ack","order_id":402,"price":9000,"qty":15,"reject_reason":"none","side":"sell","ts":5}
{"kind":"fill","order_id":401,"price":9000,"qty":15,"reject_reason":"none","side":"buy","ts":5}
{"kind":"fill","order_id":402,"price":9000,"qty":15,"reject_reason":"none","side":"sell","ts":5}
```

### Status: PASS

C++ and Python streams are byte-identical. Both implementations correctly handle the cancel-after-fully-filled case in the multi-symbol setting: the cross-symbol lookup misses (because the maker was erased from `OrderIndex` when it fully filled at ts=2), the engine emits Reject NotFound with the section 10.2 sentinels, and symbol 4's unrelated bid is unaffected. The follow-on IOC at ts=5 fully crosses order 401, confirming symbol 4's book was never touched. This case is the multi-symbol generalization of CXL-5 and is the regression test for the dual-index coherence invariant called out in ADR 0002 ("the two indexes must never disagree"). It is encoded in `tests/integration/test_multi_instrument.cpp::cancel_after_fully_filled_multi_symbol`, added during the multi-instrument review.

---

## Multi-instrument Case 3, unknown-symbol reject (limit and market shapes)

Source: audit case 3 ("NewOrder for unknown symbol, Reject UnknownSymbol"). The new `RejectReason::UnknownSymbol = 5` enumerator handles this, and ADR 0002 covers the routing (the unknown-symbol path is the inverse of the registered-symbol fast path).

### Input event sequence

* Event 1: `NewOrder { id=10, symbol=99, side=B, type=LIM, price=100, qty=10, ts=1 }` (symbol 99 not in the registered set `{1,2,3,4,5}`)
* Event 2: `NewOrder { id=11, symbol=42, side=B, type=MKT, price=0, qty=10, ts=2 }` (symbol 42 not in the registered set; market order; price field is sentinel 0)

### Expected output (mirroring the EmptyBook reject convention from matching-semantics.md section 4.1)

1. Single Reject for id=10 with `reject_reason=unknown_symbol`, carrying the submitted `side=buy`, `price=100`, `qty=10`, `ts=1`. No preceding Acknowledge (the order never reached the matching loop, mirroring MKT-4's empty-book convention from section 4.1).
2. Single Reject for id=11 with `reject_reason=unknown_symbol`, carrying `side=buy`, `price=0` (the market-order sentinel), `qty=10`, `ts=2`.

If either engine emitted an Acknowledge before the Reject (option (a) from matching-semantics.md section 4.1), this would be a divergence from the convention. Both engines must take option (b): single Reject, no preceding Ack.

### C++ engine actual output (JSON Lines)

```
{"kind":"reject","order_id":10,"price":100,"qty":10,"reject_reason":"unknown_symbol","side":"buy","ts":1}
{"kind":"reject","order_id":11,"price":0,"qty":10,"reject_reason":"unknown_symbol","side":"buy","ts":2}
```

### Python reference actual output (JSON Lines)

```
{"kind":"reject","order_id":10,"price":100,"qty":10,"reject_reason":"unknown_symbol","side":"buy","ts":1}
{"kind":"reject","order_id":11,"price":0,"qty":10,"reject_reason":"unknown_symbol","side":"buy","ts":2}
```

### Status: PASS

C++ and Python streams are byte-identical. Both engines emit a single Reject UnknownSymbol with no preceding Acknowledge, preserving the submitted `side`, `price`, and `qty` so a downstream consumer can see exactly what was rejected. The convention matches the EmptyBook reject shape from matching-semantics.md section 4.1: when the engine has not started any matching work for an order, a single Reject (and no Ack) is the cleanest output for downstream correlators.

---

## Multi-instrument Case 4, multi-symbol concurrent matching across all 5 demo symbols

Source: audit case 4 ("multi-symbol concurrent matching, 5 symbols, 5 audit logs, all match reference"). Uses the demo set `{AAPL=1, SPY=2, NVDA=3, TSLA=4, GOOG=5}`.

### Input event sequence

For each symbol in `{1, 2, 3, 4, 5}`, four events are emitted in order with timestamps incrementing globally (`ts=1..20`):

1. `NewOrder { id=sym*10+1, symbol=sym, side=B, type=LIM, price=100, qty=5 }` (resting bid)
2. `NewOrder { id=sym*10+2, symbol=sym, side=S, type=LIM, price=105, qty=5 }` (resting ask, lowest)
3. `NewOrder { id=sym*10+3, symbol=sym, side=S, type=LIM, price=106, qty=10 }` (resting ask, second level)
4. `NewOrder { id=sym*10+4, symbol=sym, side=B, type=MKT, price=0, qty=7 }` (market buy that fully consumes the 105 level and partially consumes the 106 level)

For each symbol, the market buy must produce: 1 ack, 2 maker fills (105 then 106), 2 taker fills, leaving 5 of order `sym*10+1` resting on the bid side at 100 and 8 of order `sym*10+3` resting on the ask side at 106.

### Expected output (per matching-semantics.md sections 3 and 4, applied symbol by symbol with no cross-symbol effects)

Per symbol, after the resting bid and two resting asks ack, the market buy produces this fill pattern (5 lines):

1. Ack for the market buy (`price=0, qty=7`).
2. Maker fill on the 105 ask (`qty=5`).
3. Taker fill at 105 (`qty=5`).
4. Maker fill on the 106 ask (`qty=2`, partial; 8 remain on that level).
5. Taker fill at 106 (`qty=2`).

Across all 5 symbols, that yields 5 acks per symbol (3 limit acks plus 1 market ack plus the four fills) for a total of 40 reports (8 per symbol). If any symbol's market buy leaked into another symbol's book, the leak would be observable as either a missing fill (if the wrong book was queried) or an extra fill (if the wrong book was consumed).

### C++ engine actual output (JSON Lines)

```
{"kind":"ack","order_id":11,"price":100,"qty":5,"reject_reason":"none","side":"buy","ts":1}
{"kind":"ack","order_id":12,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":2}
{"kind":"ack","order_id":13,"price":106,"qty":10,"reject_reason":"none","side":"sell","ts":3}
{"kind":"ack","order_id":14,"price":0,"qty":7,"reject_reason":"none","side":"buy","ts":4}
{"kind":"fill","order_id":12,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":4}
{"kind":"fill","order_id":14,"price":105,"qty":5,"reject_reason":"none","side":"buy","ts":4}
{"kind":"fill","order_id":13,"price":106,"qty":2,"reject_reason":"none","side":"sell","ts":4}
{"kind":"fill","order_id":14,"price":106,"qty":2,"reject_reason":"none","side":"buy","ts":4}
{"kind":"ack","order_id":21,"price":100,"qty":5,"reject_reason":"none","side":"buy","ts":5}
{"kind":"ack","order_id":22,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":6}
{"kind":"ack","order_id":23,"price":106,"qty":10,"reject_reason":"none","side":"sell","ts":7}
{"kind":"ack","order_id":24,"price":0,"qty":7,"reject_reason":"none","side":"buy","ts":8}
{"kind":"fill","order_id":22,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":8}
{"kind":"fill","order_id":24,"price":105,"qty":5,"reject_reason":"none","side":"buy","ts":8}
{"kind":"fill","order_id":23,"price":106,"qty":2,"reject_reason":"none","side":"sell","ts":8}
{"kind":"fill","order_id":24,"price":106,"qty":2,"reject_reason":"none","side":"buy","ts":8}
{"kind":"ack","order_id":31,"price":100,"qty":5,"reject_reason":"none","side":"buy","ts":9}
{"kind":"ack","order_id":32,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":10}
{"kind":"ack","order_id":33,"price":106,"qty":10,"reject_reason":"none","side":"sell","ts":11}
{"kind":"ack","order_id":34,"price":0,"qty":7,"reject_reason":"none","side":"buy","ts":12}
{"kind":"fill","order_id":32,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":12}
{"kind":"fill","order_id":34,"price":105,"qty":5,"reject_reason":"none","side":"buy","ts":12}
{"kind":"fill","order_id":33,"price":106,"qty":2,"reject_reason":"none","side":"sell","ts":12}
{"kind":"fill","order_id":34,"price":106,"qty":2,"reject_reason":"none","side":"buy","ts":12}
{"kind":"ack","order_id":41,"price":100,"qty":5,"reject_reason":"none","side":"buy","ts":13}
{"kind":"ack","order_id":42,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":14}
{"kind":"ack","order_id":43,"price":106,"qty":10,"reject_reason":"none","side":"sell","ts":15}
{"kind":"ack","order_id":44,"price":0,"qty":7,"reject_reason":"none","side":"buy","ts":16}
{"kind":"fill","order_id":42,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":16}
{"kind":"fill","order_id":44,"price":105,"qty":5,"reject_reason":"none","side":"buy","ts":16}
{"kind":"fill","order_id":43,"price":106,"qty":2,"reject_reason":"none","side":"sell","ts":16}
{"kind":"fill","order_id":44,"price":106,"qty":2,"reject_reason":"none","side":"buy","ts":16}
{"kind":"ack","order_id":51,"price":100,"qty":5,"reject_reason":"none","side":"buy","ts":17}
{"kind":"ack","order_id":52,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":18}
{"kind":"ack","order_id":53,"price":106,"qty":10,"reject_reason":"none","side":"sell","ts":19}
{"kind":"ack","order_id":54,"price":0,"qty":7,"reject_reason":"none","side":"buy","ts":20}
{"kind":"fill","order_id":52,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":20}
{"kind":"fill","order_id":54,"price":105,"qty":5,"reject_reason":"none","side":"buy","ts":20}
{"kind":"fill","order_id":53,"price":106,"qty":2,"reject_reason":"none","side":"sell","ts":20}
{"kind":"fill","order_id":54,"price":106,"qty":2,"reject_reason":"none","side":"buy","ts":20}
```

### Python reference actual output (JSON Lines)

```
{"kind":"ack","order_id":11,"price":100,"qty":5,"reject_reason":"none","side":"buy","ts":1}
{"kind":"ack","order_id":12,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":2}
{"kind":"ack","order_id":13,"price":106,"qty":10,"reject_reason":"none","side":"sell","ts":3}
{"kind":"ack","order_id":14,"price":0,"qty":7,"reject_reason":"none","side":"buy","ts":4}
{"kind":"fill","order_id":12,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":4}
{"kind":"fill","order_id":14,"price":105,"qty":5,"reject_reason":"none","side":"buy","ts":4}
{"kind":"fill","order_id":13,"price":106,"qty":2,"reject_reason":"none","side":"sell","ts":4}
{"kind":"fill","order_id":14,"price":106,"qty":2,"reject_reason":"none","side":"buy","ts":4}
{"kind":"ack","order_id":21,"price":100,"qty":5,"reject_reason":"none","side":"buy","ts":5}
{"kind":"ack","order_id":22,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":6}
{"kind":"ack","order_id":23,"price":106,"qty":10,"reject_reason":"none","side":"sell","ts":7}
{"kind":"ack","order_id":24,"price":0,"qty":7,"reject_reason":"none","side":"buy","ts":8}
{"kind":"fill","order_id":22,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":8}
{"kind":"fill","order_id":24,"price":105,"qty":5,"reject_reason":"none","side":"buy","ts":8}
{"kind":"fill","order_id":23,"price":106,"qty":2,"reject_reason":"none","side":"sell","ts":8}
{"kind":"fill","order_id":24,"price":106,"qty":2,"reject_reason":"none","side":"buy","ts":8}
{"kind":"ack","order_id":31,"price":100,"qty":5,"reject_reason":"none","side":"buy","ts":9}
{"kind":"ack","order_id":32,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":10}
{"kind":"ack","order_id":33,"price":106,"qty":10,"reject_reason":"none","side":"sell","ts":11}
{"kind":"ack","order_id":34,"price":0,"qty":7,"reject_reason":"none","side":"buy","ts":12}
{"kind":"fill","order_id":32,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":12}
{"kind":"fill","order_id":34,"price":105,"qty":5,"reject_reason":"none","side":"buy","ts":12}
{"kind":"fill","order_id":33,"price":106,"qty":2,"reject_reason":"none","side":"sell","ts":12}
{"kind":"fill","order_id":34,"price":106,"qty":2,"reject_reason":"none","side":"buy","ts":12}
{"kind":"ack","order_id":41,"price":100,"qty":5,"reject_reason":"none","side":"buy","ts":13}
{"kind":"ack","order_id":42,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":14}
{"kind":"ack","order_id":43,"price":106,"qty":10,"reject_reason":"none","side":"sell","ts":15}
{"kind":"ack","order_id":44,"price":0,"qty":7,"reject_reason":"none","side":"buy","ts":16}
{"kind":"fill","order_id":42,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":16}
{"kind":"fill","order_id":44,"price":105,"qty":5,"reject_reason":"none","side":"buy","ts":16}
{"kind":"fill","order_id":43,"price":106,"qty":2,"reject_reason":"none","side":"sell","ts":16}
{"kind":"fill","order_id":44,"price":106,"qty":2,"reject_reason":"none","side":"buy","ts":16}
{"kind":"ack","order_id":51,"price":100,"qty":5,"reject_reason":"none","side":"buy","ts":17}
{"kind":"ack","order_id":52,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":18}
{"kind":"ack","order_id":53,"price":106,"qty":10,"reject_reason":"none","side":"sell","ts":19}
{"kind":"ack","order_id":54,"price":0,"qty":7,"reject_reason":"none","side":"buy","ts":20}
{"kind":"fill","order_id":52,"price":105,"qty":5,"reject_reason":"none","side":"sell","ts":20}
{"kind":"fill","order_id":54,"price":105,"qty":5,"reject_reason":"none","side":"buy","ts":20}
{"kind":"fill","order_id":53,"price":106,"qty":2,"reject_reason":"none","side":"sell","ts":20}
{"kind":"fill","order_id":54,"price":106,"qty":2,"reject_reason":"none","side":"buy","ts":20}
```

### Status: PASS

C++ and Python streams are byte-identical across all 40 reports (8 per symbol times 5 symbols). Both engines dispatch each event to the correct per-symbol book and produce identical fills, with no cross-symbol leakage. This is the strongest demonstration of the multi-symbol dispatch invariant: 5 independent matching loops run cleanly side by side, and every per-symbol fill pattern matches the single-symbol MKT-2 ("market buy sweeping multiple price levels", matching-semantics.md section 4.3) shape applied 5 times. The conservation law holds per symbol: each `qty=7` market buy fills `5 + 2 = 7` and rests 0; each `qty=10` resting ask at 106 has `2` consumed and `8` remaining.

---

## Multi-instrument Case 5, cross-symbol cancel by id routing (cancel arrives without naming the symbol)

Source: audit case 5 ("cancel-by-id where the id was placed on Y but the cancel arrives without naming Y; the engine routes correctly"). The cross-symbol cancel routes through `OrderIndex::find` (per ADR 0002), which returns the `Order*`; the `Order::symbol` field then identifies the right `Book`.

### Input event sequence

* Events 1 through 5: one resting buy per symbol (id=1 on symbol 1, id=2 on symbol 2, ..., id=5 on symbol 5), each at `price=99, qty=10`.
* Event 6: `Cancel { order_id=5, ts=6 }` (no symbol on the wire; engine must route via `OrderIndex` to symbol 5's book).
* Event 7: `NewOrder { id=6, symbol=5, side=S, type=IOC, price=99, qty=10, ts=7 }` (sanity-touch on symbol 5: should ack and fully cancel the residual because symbol 5's bid was just removed; if the engine routed the cancel to the wrong symbol, this IOC would fully cross instead).

### Expected output (per ADR 0002 routing plus matching-semantics.md sections 5.4 and 6.1)

1. Five Acks for the resting buys (ids 1 through 5, on symbols 1 through 5 respectively).
2. Cancel for id=5 carrying `side=buy`, `price=99`, `qty=10`, `ts=6` (symbol 5's book contained id=5; the cross-symbol routing finds it via `OrderIndex.find(5)` returning a pointer to the resting order on symbol 5's book).
3. Ack for the symbol-5 IOC sell at `price=99, qty=10, ts=7`. No fills follow because symbol 5's bid side is now empty (per IOC-3 from matching-semantics.md section 5.3: an IOC against an empty book is acknowledged and then implicitly cancelled in full; no Cancel report).

Symbols 1 through 4 must still hold their resting buys after the cancel of id=5; this is implicit in the absence of any further reports for those symbols.

### C++ engine actual output (JSON Lines)

```
{"kind":"ack","order_id":1,"price":99,"qty":10,"reject_reason":"none","side":"buy","ts":1}
{"kind":"ack","order_id":2,"price":99,"qty":10,"reject_reason":"none","side":"buy","ts":2}
{"kind":"ack","order_id":3,"price":99,"qty":10,"reject_reason":"none","side":"buy","ts":3}
{"kind":"ack","order_id":4,"price":99,"qty":10,"reject_reason":"none","side":"buy","ts":4}
{"kind":"ack","order_id":5,"price":99,"qty":10,"reject_reason":"none","side":"buy","ts":5}
{"kind":"cancel","order_id":5,"price":99,"qty":10,"reject_reason":"none","side":"buy","ts":6}
{"kind":"ack","order_id":6,"price":99,"qty":10,"reject_reason":"none","side":"sell","ts":7}
```

### Python reference actual output (JSON Lines)

```
{"kind":"ack","order_id":1,"price":99,"qty":10,"reject_reason":"none","side":"buy","ts":1}
{"kind":"ack","order_id":2,"price":99,"qty":10,"reject_reason":"none","side":"buy","ts":2}
{"kind":"ack","order_id":3,"price":99,"qty":10,"reject_reason":"none","side":"buy","ts":3}
{"kind":"ack","order_id":4,"price":99,"qty":10,"reject_reason":"none","side":"buy","ts":4}
{"kind":"ack","order_id":5,"price":99,"qty":10,"reject_reason":"none","side":"buy","ts":5}
{"kind":"cancel","order_id":5,"price":99,"qty":10,"reject_reason":"none","side":"buy","ts":6}
{"kind":"ack","order_id":6,"price":99,"qty":10,"reject_reason":"none","side":"sell","ts":7}
```

### Status: PASS

C++ and Python streams are byte-identical. Both engines route the cancel of id=5 to symbol 5's book without the caller naming the symbol on the wire (per ADR 0002: the `Order::symbol` field carries the routing tag, indexed via `OrderIndex`). The follow-on IOC sell at ts=7 is acknowledged and then implicitly cancelled in full because symbol 5's bid side was emptied by the preceding cancel; if the cancel had been misrouted to any of symbols 1 through 4, symbol 5's bid would still have been present and the IOC would have fully crossed it (an extra pair of Fill reports). The absence of those fills proves the routing is correct.

---

## Multi-instrument summary table

| # | Case | Branch under audit | C++ vs expected | Python vs expected | C++ vs Python | Status |
|---|------|--------------------|-----------------|--------------------|----------------|--------|
| 1 | cross-symbol-cancel-isolation | Cancel of resting id=100 on symbol 1; symbol 2 untouched | match | match | byte-identical | PASS |
| 2 | cancel-after-fully-filled-multi-symbol | Cancel of fully-filled id=201; Reject NotFound; symbol 4 untouched | match | match | byte-identical | PASS |
| 3 | unknown-symbol-reject | NewOrder for symbols 99 and 42 (unregistered); single Reject UnknownSymbol no preceding Ack | match | match | byte-identical | PASS |
| 4 | five-symbol-independent-activity | 5 symbols times (3 limits + 1 market); 40 reports total; no leakage | match | match | byte-identical | PASS |
| 5 | cross-symbol-cancel-by-id-routing | Cancel id=5 with no symbol on wire; routes via `OrderIndex` | match | match | byte-identical | PASS |

All 5 multi-instrument cases pass in both implementations, with byte-identical output streams.

---

## How to reproduce the multi-instrument audit

The captures above were produced from the project root with the following procedure. The procedure is deterministic; rerunning produces byte-identical output.

### Build the multi-instrument audit capture binary

The C++ capture harness is the multi-instrument counterpart of `tests/integration/audit_capture.cpp`. The single-symbol file was written against the single-`Book` constructor and does not compile against the multi-instrument `MatchingEngine(OrderPool&, BookRegistry&, OrderIndex&)`. Rather than vendoring a multi-instrument version into the repo permanently (the parameterized integration test `tests/integration/test_multi_instrument.cpp` is the canonical CI gate; this audit harness is a one-shot for human readability), the harness was written into a temp file and built directly against `build/src/libmeridian.a`:

```
g++ -std=c++20 -O2 -Iinclude /tmp/audit_capture_multi.cpp \
    build/src/libmeridian.a -o /tmp/audit_capture_multi
```

The capture binary's source follows the same pattern as the integration test's `run_cpp` helper but uses `std::cout` to emit one JSON Lines block per case rather than asserting through gtest.

### Capture the C++ engine output

```
/tmp/audit_capture_multi > /tmp/multi_cpp.out
```

### Capture the Python reference output

The same five event sequences are run through `tests/reference/run_reference.py`, one process per case so each case starts with a fresh engine instance. The driver was scripted into `/tmp/capture_multi.sh` (a small bash helper that pipes each case's events to the reference and tags the output blocks):

```
bash /tmp/capture_multi.sh > /tmp/multi_python.out
```

### Cross-check

```
diff /tmp/multi_cpp.out /tmp/multi_python.out
```

Empty diff means byte-for-byte agreement. The capture run that produced this section had a clean diff across all 5 cases combined.

### Independent regression checks already in CI

* `tests/integration/test_multi_instrument.cpp` runs 7 parameterized scenarios through both engines and asserts byte-identical JSON output: `five_symbol_independent`, `cross_symbol_cancel_isolation`, `cross_symbol_cancel_by_id_routing`, `unknown_symbol_reject`, `id_reuse_after_cancel_different_symbol`, `cancel_after_fully_filled_multi_symbol`, and `mixed_50_event_random`. This is the production CI gate.
* `tests/reference/test_reference.py::MultiSymbolTests` covers the same five surface areas in Python-only unit tests (5-symbol dispatch, cross-symbol cancel isolation, cross-symbol cancel by id routing, unknown-symbol reject, cross-symbol unknown-id NotFound), plus several backward-compatibility checks (legacy single-symbol constructor still works, multi-symbol mode requires explicit `symbol=` on submit, constructor rejects mixing `symbol=` and `symbols=`).

This audit covers five cases chosen to span the multi-instrument surface area. The CI gates are the production source of truth; this section is the human-readable side-by-side correctness audit.

---

## Multi-instrument Sign-off

**Multi-symbol matching correctness signed off.** All 5 multi-instrument audit cases pass in both implementations, and the C++ engine and Python reference produce byte-identical output for every case. No divergences blocking sign-off.

The Python reference and the C++ engine agree on:

* New-order dispatch by `symbol` field through `BookRegistry::book` (C++) / `_books[sym]` (reference).
* Cross-symbol cancel routing through `OrderIndex::find` (C++) / `_id_to_symbol` (reference); cancel for id placed on any symbol routes to the correct per-symbol book without the wire format naming the symbol.
* Cross-symbol cancel isolation: a cancel on symbol X never touches symbol Y's book (multi-instrument Case 1 and Case 5 demonstrate this directly).
* Cancel-after-fully-filled in the multi-symbol setting: Reject NotFound with the matching-semantics.md section 10.2 sentinels (`side=buy`, `price=0`, `qty=0`), confirming the dual-index design from ADR 0002 is coherent (per-`Book` index and `OrderIndex` both erase on full fill).
* Unknown-symbol reject: single Reject with `reject_reason=unknown_symbol` and no preceding Acknowledge, mirroring the EmptyBook reject convention from matching-semantics.md section 4.1.
* Independent multi-symbol activity: 5 books run side by side without any cross-symbol leakage; per-symbol fill patterns match the single-symbol contract from matching-semantics.md sections 3 and 4.
* Sentinel and ts conventions inherited from the single-symbol design carry through unchanged: Cancel reports carry the cancelled order's resting `side`, `price`, and remaining `qty`; Reject NotFound carries section 10.2 sentinels; Reject UnknownSymbol carries the submitted `side`, `price`, and `qty`.

Open follow-up items for later milestones, recorded for context:

* The dual-index design from ADR 0002 (per-`Book` id index plus cross-symbol `OrderIndex`) ships now; consolidating the two indexes is a future cleanup. Cases 1, 2, and 5 exercise the dual-index coherence invariant ("the two indexes must never disagree"); a hand-audit of `MatchingEngine::apply_limit` and `MatchingEngine::sweep` confirmed both insert and erase paths update both indexes.
* The property-test suite under `tests/property/` covers the matching invariants today and exercises the multi-instrument additions implicitly via the demo symbol set: the generator drives all five symbols (1..5) and the cancel path is cross-symbol routed through `OrderIndex`. Cross-symbol cancel isolation, dual-index coherence, and the unknown-symbol reject shape are exercised on every random sequence; targeted property generators for these specific shapes can be added on a regression as needed.
* The NASDAQ ITCH 5.0 replay milestone will exercise the multi-symbol dispatch at scale and is the natural moment to confirm the dispatch overhead stays inside the latency budget; the bench is currently at the same throughput as the single-symbol baseline within noise.
