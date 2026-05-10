# tests/reference

Python reference implementations used as ground truth for the Meridian C++ matching engine. Owned by the Reference Implementation Engineer (agent 18). Implements the Phase 1 contract from `docs/risk/matching-semantics.md` (single-symbol limit, market, IOC, and cancel-by-id). Phase 6 will add `itch_reference.py`; Phase 7 will extend the matching engine with PostOnly and FOK semantics.

## Files

| File | Purpose |
|---|---|
| `matching_reference.py` | Single-symbol price-time-priority matching engine. Stdlib only. About 460 lines including docstrings. |
| `test_reference.py` | Worked-example unit tests; one test per worked example in `docs/risk/matching-semantics.md`, plus determinism and conservation-law sanity checks. |
| `run_reference.py` | CLI driver. Reads JSON Lines events from stdin, writes JSON Lines reports to stdout, byte-comparable against the C++ engine. |

## How to run the tests

From the repo root:

```bash
python3 -m unittest discover tests/reference -v
```

Expected: 29 tests, all pass (26 worked examples plus 2 conservation-law sanity checks plus 1 determinism check).

The suite has zero third-party dependencies. Python 3.11 or newer is sufficient (the project's CI sets up 3.11 explicitly; the file uses `from __future__ import annotations` so the type hints work on any 3.10 or newer interpreter).

## How to run the reference standalone

The CLI driver reads JSON Lines events from stdin, drives the reference matching engine, and writes one JSON Lines report per emitted `ExecutionReport` to stdout. Both streams are deterministic and byte-stable across runs (the determinism unit test checks this).

```bash
# From the repo root, with PYTHONPATH set so the import resolves:
echo '{"kind":"new_order","type":"limit","order_id":1,"side":"buy","price":10000,"qty":10,"ts":1}' \
    | PYTHONPATH=tests/reference python3 tests/reference/run_reference.py
```

Or interactively, from inside `tests/reference/`:

```bash
cd tests/reference
python3 run_reference.py < input.jsonl > output.jsonl
diff -u expected.jsonl output.jsonl
```

The Phase 1 integration test (`tests/integration/test_engine_vs_reference.cpp`, written by the QA Engineer in Task 15 of the Phase 1 plan) writes the same JSON Lines input file, runs both the C++ engine and `run_reference.py`, and asserts the two output streams match line by line.

### Wire format

Input events (one JSON object per line):

```
{"kind": "new_order", "type": "limit"|"market"|"ioc",
 "order_id": <int>, "side": "buy"|"sell",
 "price": <int>, "qty": <int>, "ts": <int>}

{"kind": "cancel", "order_id": <int>, "ts": <int>}
```

For market orders, `price` is unused by the engine but should be present in the JSON for schema regularity (the matching-semantics worked examples write `price=0` as a sentinel; see section 4.2 MKT-1).

Output reports (one JSON object per line, keys in alphabetical order so the C++ and Python sides produce identical bytes):

```
{"kind": "ack"|"fill"|"cancel"|"reject",
 "order_id": <int>, "price": <int>, "qty": <int>,
 "reject_reason": "none"|"empty_book"|"not_found"|"insufficient_liquidity"|"would_cross",
 "side": "buy"|"sell", "ts": <int>}
```

The `reject_reason` field is emitted on every report (not only on rejects) so the JSON shape is uniform across kinds and the diff harness compares lines without branching. Non-reject reports carry `"none"`.

### Cancel report timestamps

The companion plan's Task 14 step 1 sketch of `run_reference.py` calls `engine.cancel(order_id)` with no timestamp. The reference here calls `engine.cancel_at(order_id, ts)` instead so the emitted `Cancel` (or `Reject NotFound`) report's `ts` matches the C++ engine's output. This is a small enhancement of the sketch; the wire format adds a `ts` to the cancel event JSON for parity with the new-order events.

## Design rationale: obvious over fast

The reference is intentionally slow and intentionally simple. From the agent brief (`agents/18-reference-implementation-engineer.md`, "Design constraints"):

> **Obvious over fast.** The reference uses dicts, lists, and integer prices. No micro-optimization. If the reference takes 100x as long as the C++ engine, that is fine; the integration test runs on a small sample tape.

Concretely:

* The bid and ask sides are plain `dict[Price, _Level]` and the matching loop walks them via `min(...)` and `max(...)` on each iteration. The C++ engine uses `std::map<Price, Level*>` for O(log n) ordered traversal; the reference does not bother. For a Phase 1 integration test corpus of a few thousand events, the cost is negligible.
* Each `_Level` holds an `OrderedDict[OrderId, _RestingOrder]` for the FIFO queue at that price. Insertion appends to the tail; `next(iter(...))` returns the front (oldest resting order). This is the literal reading of the price-time priority rule (matching-semantics.md section 2.2).
* Every report serializes via `dataclasses.asdict`-style `to_dict()` and `json.dumps(..., sort_keys=True)`. No custom serializer, no streaming. Determinism is enforced by stdlib alone.
* No threads, no atomics, no concurrency. The reference is single-threaded by construction; the C++ engine's seqlock-protected publication path lands in Phase 4 and is reviewed against the reference's "what should the matching loop have produced?" output, not against an in-process Python reader.
* No exceptions on the matching loop path beyond `ValueError` for invalid input quantities; the reference treats `qty <= 0` as a programming error rather than a runtime reject. The C++ side guards on submission boundary; the reference matches.

When the reference and the C++ engine disagree, the reference is presumed correct unless the disagreement is a known reference bug (`agents/18-reference-implementation-engineer.md`, "Mission"). The Risk and Financial Correctness Reviewer adjudicates persistent disagreements.

## Spec-section citations

Every public-API rule in `matching_reference.py` cites the section of `docs/risk/matching-semantics.md` it implements. The mapping below is the index for cross-checking.

| Public API | Behavior | Cited section |
|---|---|---|
| `submit_limit` | Always emits `Acknowledge` first, then walks opposite side while next-maker price satisfies limit, then rests residual at limit price. | matching-semantics.md section 3 (rules) and sections 3.1 through 3.7 (worked examples LIM-1 through LIM-7). |
| `submit_market` | Single `Reject` with `reason=EmptyBook` if opposite side empty (no preceding `Acknowledge`). Otherwise `Acknowledge` then walk opposite side with no price limit; residual implicitly cancelled. | matching-semantics.md section 4 (rules) and sections 4.1 (convention), 4.2 through 4.7 (worked examples MKT-1 through MKT-6). |
| `submit_ioc` | Always emits `Acknowledge` first (even against empty book or non-crossing price), walks opposite side while limit satisfied, residual implicitly cancelled (no `Cancel` report). | matching-semantics.md section 5 (rules) and sections 5.1 through 5.7 (worked examples IOC-1 through IOC-7). |
| `cancel` / `cancel_at` | Emits `Cancel` carrying remaining qty if id is in the lookup map; emits `Reject NotFound` with sentinel side `Buy` and price `0` otherwise. Idempotent: a second cancel of the same id is a `Reject NotFound`. | matching-semantics.md section 6 (rules) and sections 6.1 through 6.6 (worked examples CXL-1 through CXL-6). |
| `top_of_book` | Returns `(best_bid_px, best_bid_qty, best_ask_px, best_ask_qty)`. Empty side returns `(None, 0)` for that pair. | Used by the future demo binary; not part of the matching-semantics worked examples. Internal API. |

Two convention choices were ratified in `matching-semantics.md` section 8 and are encoded here verbatim:

* **Section 8.1**: market against empty book emits a single `Reject` with no preceding `Acknowledge`. Implemented in `submit_market`.
* **Section 8.2**: cancel of an unknown id emits a `Reject` with sentinel side `Side.BUY` and price `0`. Implemented in `cancel_at`.

If the PM later overrides either of these, the relevant unit tests in `test_reference.py` and the matching code in `matching_reference.py` need to be updated together; both this README and the matching-semantics worked examples have to move at the same time.

## What's not in the reference (Phase 1 scope discipline)

* `OrderType.POSTONLY` and `OrderType.FOK` are declared in the enum for parity with the C++ side but are not implemented; calling `submit_*` for them is not a code path. Phase 7 adds these.
* No multi-symbol dispatch. `MatchingReference(symbol=int)` ignores the symbol argument in Phase 1; Phase 2 adds a `BookRegistry`-equivalent.
* No ITCH parsing. Phase 6 adds `tests/reference/itch_reference.py`.
* No property-based tests. Phase 3 wires `rapidcheck` into the C++ build and pairs the generated corpus against the reference; the reference itself does not need to import any property-testing library because the C++ harness produces the events.

## Handoffs

* The C++ test integration (`tests/integration/test_engine_vs_reference.cpp`) is the QA Engineer's deliverable in Task 15 of the Phase 1 plan. It calls `run_reference.py` via subprocess and diffs.
* Disagreements between the reference and the C++ engine are escalated to the Risk and Financial Correctness Reviewer (agent 16), who hand-audits the disagreement against `docs/risk/matching-semantics.md` and decides which side has the bug.
* Semantic-correctness questions (e.g., "should an IOC at a non-crossing price emit zero fills or a Reject?") go to the Quant Domain Validator (agent 01), who owns `docs/risk/matching-semantics.md`.
