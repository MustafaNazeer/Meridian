"""Drive the Python matching reference from an ITCH 5.0 binary tape.

Mirror of ``apps/replay/main.cpp``. Reads a tape, dispatches Add Order
('A') and Order Delete ('D') messages to ``MatchingReference``, writes
the resulting JSON Lines audit stream to stdout.

Used by the integration test ``tests/integration/test_itch_replay.cpp``
to byte-diff the C++ engine's audit output against the Python
reference's audit output on the same input tape.

Usage::

    python3 tests/reference/itch_replay.py --tape PATH > audit.jsonl

Single-symbol scope: every distinct ITCH stock id maps to symbol id 1,
matching the C++ side.
"""

from __future__ import annotations

import argparse
import json
import sys

from itch_parser import MessageKind, ParseStatus, parse_stream
from matching_reference import MatchingReference, Side


def main() -> int:
    ap = argparse.ArgumentParser(description="Replay an ITCH tape through the Python matching reference")
    ap.add_argument("--tape", required=True, help="ITCH 5.0 binary tape path")
    args = ap.parse_args()

    with open(args.tape, "rb") as f:
        tape = f.read()

    sym = 1
    engine = MatchingReference(symbols=[sym])
    out = sys.stdout

    for status, msg, _cursor in parse_stream(tape):
        if status == ParseStatus.END_OF_STREAM:
            break
        if status != ParseStatus.OK:
            sys.stderr.write(f"parser error: {status.value}\n")
            return 1
        assert msg is not None
        reports: list = []
        if msg.kind == MessageKind.ADD_ORDER:
            assert msg.add is not None
            side = Side.BUY if msg.add.side == "buy" else Side.SELL
            reports = engine.submit_limit(
                msg.add.order_ref, side, msg.add.price, msg.add.shares,
                msg.add.ts_ns, symbol=sym,
            )
        elif msg.kind == MessageKind.ORDER_DELETE:
            assert msg.delete is not None
            reports = engine.cancel_at(msg.delete.order_ref, ts=msg.delete.ts_ns)
        else:
            continue
        for report in reports:
            out.write(json.dumps(report.to_dict(), sort_keys=True,
                                 separators=(",", ":")))
            out.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
