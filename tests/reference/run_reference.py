"""Drive the matching reference from a JSON Lines event stream on stdin.

The Phase 1 integration test (``tests/integration/test_engine_vs_reference.cpp``)
writes a JSON Lines event file, runs the C++ engine, runs this script
on the same input, and byte-diffs the two output streams. Both outputs
are JSON Lines, one ``ExecutionReport`` per line, with object keys
emitted in alphabetical order so the diff is line-by-line.

Wire format
-----------

Input (one JSON object per line, all fields required for new orders;
``price`` may be 0 for market orders):

    {"kind": "new_order", "type": "limit"|"market"|"ioc",
     "order_id": <int>, "side": "buy"|"sell",
     "price": <int>, "qty": <int>, "ts": <int>}

    {"kind": "cancel", "order_id": <int>, "ts": <int>}

The cancel event carries a ``ts`` so the emitted ``Cancel`` (or
``Reject NotFound``) report's timestamp matches the C++ engine's
output. The companion plan Task 14 step 1 sketch dropped ``ts`` from
the cancel call; that sketch is incomplete and would cause a
spurious diff against the C++ output. The reference passes ``ts``
through via ``MatchingReference.cancel_at`` so the diff is clean.

Output (one JSON object per line, sorted keys):

    {"kind": "ack"|"fill"|"cancel"|"reject",
     "order_id": <int>, "price": <int>, "qty": <int>,
     "reject_reason": "none"|"empty_book"|"not_found"|...,
     "side": "buy"|"sell", "ts": <int>}

Run
---

Standalone, from the repo root::

    echo '{"kind":"new_order","type":"limit","order_id":1,"side":"buy",
           "price":10000,"qty":10,"ts":1}' \
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
        if otype == "limit":
            return engine.submit_limit(order_id, side, price, qty, ts)
        if otype == "market":
            return engine.submit_market(order_id, side, qty, ts)
        if otype == "ioc":
            return engine.submit_ioc(order_id, side, price, qty, ts)
        raise ValueError(f"unknown order type {otype!r}")
    if kind == "cancel":
        return engine.cancel_at(event["order_id"], ts=event["ts"])
    raise ValueError(f"unknown event kind {kind!r}")


def main() -> int:
    engine = MatchingReference(symbol=1)
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
