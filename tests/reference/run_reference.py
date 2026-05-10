"""Drive the matching reference from a JSON Lines event stream on stdin.

The Phase 1 integration test (``tests/integration/test_engine_vs_reference.cpp``)
writes a JSON Lines event file, runs the C++ engine, runs this script
on the same input, and byte-diffs the two output streams. Both outputs
are JSON Lines, one ``ExecutionReport`` per line, with object keys
emitted in alphabetical order so the diff is line-by-line.

Wire format
-----------

Input (one JSON object per line). New-order events:

    {"kind": "new_order", "type": "limit"|"market"|"ioc",
     "order_id": <int>, "side": "buy"|"sell",
     "price": <int>, "qty": <int>, "ts": <int>,
     "symbol": <int>}

The ``symbol`` field is optional in Phase 1 scenarios (which all use
symbol id 1 implicitly). When absent, the driver defaults the new-order
event to ``symbol=1`` so the Phase 1 integration corpus continues to
diff cleanly. Phase 2 scenarios that drive multiple symbols always
populate ``symbol`` explicitly. Phase 2 plan Task 2 step 4 pins this
default behavior.

Cancel events:

    {"kind": "cancel", "order_id": <int>, "ts": <int>}

Cancels do not carry ``symbol``. The reference looks up the order's
symbol in its cross-symbol id-to-symbol map and routes the cancel to
the correct per-symbol book. An unknown id produces ``Reject NOT_FOUND``
(matching-semantics.md section 6).

The cancel event carries a ``ts`` so the emitted ``Cancel`` (or
``Reject NOT_FOUND``) report's timestamp matches the C++ engine's
output.

Output (one JSON object per line, sorted keys):

    {"kind": "ack"|"fill"|"cancel"|"reject",
     "order_id": <int>, "price": <int>, "qty": <int>,
     "reject_reason": "none"|"empty_book"|"not_found"|"unknown_symbol"|...,
     "side": "buy"|"sell", "ts": <int>}

Run
---

Standalone, from the repo root::

    echo '{"kind":"new_order","type":"limit","order_id":1,"side":"buy",
           "price":10000,"qty":10,"ts":1,"symbol":1}' \
        | PYTHONPATH=tests/reference python3 tests/reference/run_reference.py

Or from inside the ``tests/reference`` directory::

    cd tests/reference
    python3 run_reference.py < input.jsonl > output.jsonl

The integration test sets ``PYTHONPATH`` to ``tests/reference`` so the
``matching_reference`` module imports cleanly regardless of the
working directory.
"""

from __future__ import annotations

import json
import sys

from matching_reference import MatchingReference, Side


_SIDE_MAP = {"buy": Side.BUY, "sell": Side.SELL}

# Phase 2 demo symbol set: AAPL=1, SPY=2, NVDA=3, TSLA=4, GOOG=5
# (matches `BookRegistry` initializer in the C++ Phase 2 fixture, per
# the Phase 2 plan Task 5 dispatch flow). Phase 1 corpora that omit
# the ``symbol`` field default to symbol=1, which is in this set.
_DEFAULT_SYMBOLS: tuple[int, ...] = (1, 2, 3, 4, 5)


def _process_event(engine: MatchingReference, event: dict) -> list:
    """Dispatch one decoded event to the engine and return its reports."""

    kind = event["kind"]
    if kind == "new_order":
        otype = event["type"]
        side = _SIDE_MAP[event["side"]]
        order_id = event["order_id"]
        price = event["price"]
        qty = event["qty"]
        ts = event["ts"]
        # Phase 2: symbol is optional; default to 1 for Phase 1 backward
        # compatibility. The integration corpus from Phase 1 omits it.
        symbol = event.get("symbol", 1)
        if otype == "limit":
            return engine.submit_limit(
                order_id, side, price, qty, ts, symbol=symbol
            )
        if otype == "market":
            return engine.submit_market(
                order_id, side, qty, ts, symbol=symbol
            )
        if otype == "ioc":
            return engine.submit_ioc(
                order_id, side, price, qty, ts, symbol=symbol
            )
        raise ValueError(f"unknown order type {otype!r}")
    if kind == "cancel":
        # Cancel is cross-symbol: the engine routes via its
        # id-to-symbol map. The wire format does not carry symbol on
        # cancel events (Phase 2 plan Task 2 step 4).
        return engine.cancel_at(event["order_id"], ts=event["ts"])
    raise ValueError(f"unknown event kind {kind!r}")


def main() -> int:
    engine = MatchingReference(symbols=_DEFAULT_SYMBOLS)
    out = sys.stdout
    # Compact JSON (no spaces around separators) so the byte-diff against
    # the C++ engine's hand-rolled JSON output is exact. Sort keys so the
    # field order matches the C++ side's alphabetical emission.
    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue
        event = json.loads(raw)
        for report in _process_event(engine, event):
            out.write(json.dumps(report.to_dict(), sort_keys=True,
                                 separators=(",", ":")))
            out.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
