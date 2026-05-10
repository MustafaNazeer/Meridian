#!/usr/bin/env python3
"""Compare a meridian-bench JSON sidecar against a checked-in baseline.

Usage::

    python3 bench/check_regression.py BASELINE LAST_RUN

Exit codes:

    0   No regression detected, OR baseline file is missing (treated as
        a no-op so the very first CI run before a baseline is committed
        does not fail; a warning is printed in that case).
    1   Regression detected: throughput dropped by more than the
        throughput tolerance, or a tracked latency percentile rose by
        more than the latency tolerance.
    2   Argument or schema error.

Tolerances (matching docs/perf/budget.md "How this budget is enforced"):

    Throughput:  current must be >= baseline * (1 - 0.05)  (5 percent drop allowed)
    Latency p50: current must be <= baseline * (1 + 0.10)  (10 percent rise allowed)
    Latency p99: same 10 percent rise allowance
    Latency p99.9: same 10 percent rise allowance

Both files use schema_version 2 (the bench's own JSON sidecar format).
"""

from __future__ import annotations

import json
import os
import sys
from typing import Any

THROUGHPUT_DROP_TOLERANCE = 0.05  # 5 percent
LATENCY_RISE_TOLERANCE = 0.10     # 10 percent
TRACKED_PERCENTILES = ("p50", "p99", "p99_9")
SUPPORTED_SCHEMA = 2


def load(path: str) -> dict[str, Any]:
    with open(path, "r") as f:
        data = json.load(f)
    if not isinstance(data, dict):
        raise ValueError(f"{path}: top-level value must be a JSON object")
    sv = data.get("schema_version")
    if sv != SUPPORTED_SCHEMA:
        raise ValueError(
            f"{path}: schema_version {sv!r} is not supported (expected {SUPPORTED_SCHEMA})"
        )
    return data


def fmt_pct(delta: float) -> str:
    sign = "+" if delta >= 0 else ""
    return f"{sign}{delta * 100:.2f}%"


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        sys.stderr.write(
            "usage: check_regression.py BASELINE LAST_RUN\n"
        )
        return 2
    baseline_path, last_path = argv[1], argv[2]

    if not os.path.exists(baseline_path):
        sys.stderr.write(
            f"warning: baseline file '{baseline_path}' does not exist; "
            "skipping regression check. The first CI run on this branch "
            "is expected to print this message; commit the captured "
            "baseline.json from this run so subsequent PRs can be gated.\n"
        )
        return 0

    try:
        baseline = load(baseline_path)
        last = load(last_path)
    except (OSError, ValueError, json.JSONDecodeError) as e:
        sys.stderr.write(f"error: {e}\n")
        return 2

    failures: list[str] = []
    rows: list[tuple[str, str, str, str]] = []

    base_tput = float(baseline.get("throughput_mevents_per_sec", 0.0))
    last_tput = float(last.get("throughput_mevents_per_sec", 0.0))
    tput_min = base_tput * (1.0 - THROUGHPUT_DROP_TOLERANCE)
    tput_delta = (last_tput - base_tput) / base_tput if base_tput > 0 else 0.0
    tput_status = "OK"
    if last_tput < tput_min:
        tput_status = "FAIL"
        failures.append(
            f"throughput {last_tput:.3f} M evt/s is below "
            f"floor {tput_min:.3f} M evt/s "
            f"(baseline {base_tput:.3f}, delta {fmt_pct(tput_delta)}, "
            f"tolerance {-THROUGHPUT_DROP_TOLERANCE * 100:.0f}%)"
        )
    rows.append(
        ("throughput (M evt/s)", f"{base_tput:.3f}", f"{last_tput:.3f}",
         f"{fmt_pct(tput_delta)} {tput_status}")
    )

    base_lat = baseline.get("latency_ns", {})
    last_lat = last.get("latency_ns", {})
    for pct in TRACKED_PERCENTILES:
        base_v = float(base_lat.get(pct, 0.0))
        last_v = float(last_lat.get(pct, 0.0))
        lat_max = base_v * (1.0 + LATENCY_RISE_TOLERANCE)
        delta = (last_v - base_v) / base_v if base_v > 0 else 0.0
        status = "OK"
        if base_v > 0 and last_v > lat_max:
            status = "FAIL"
            failures.append(
                f"latency {pct} {last_v:.0f} ns exceeds ceiling "
                f"{lat_max:.0f} ns "
                f"(baseline {base_v:.0f}, delta {fmt_pct(delta)}, "
                f"tolerance +{LATENCY_RISE_TOLERANCE * 100:.0f}%)"
            )
        rows.append(
            (f"latency {pct} (ns)", f"{base_v:.0f}", f"{last_v:.0f}",
             f"{fmt_pct(delta)} {status}")
        )

    sys.stdout.write("\n")
    sys.stdout.write(f"Bench regression check: {baseline_path} vs {last_path}\n")
    sys.stdout.write("\n")
    headers = ("metric", "baseline", "current", "delta")
    widths = [
        max(len(h), max(len(r[i]) for r in rows)) for i, h in enumerate(headers)
    ]
    fmt = "  ".join("{:<" + str(w) + "}" for w in widths) + "\n"
    sys.stdout.write(fmt.format(*headers))
    sys.stdout.write(fmt.format(*("-" * w for w in widths)))
    for r in rows:
        sys.stdout.write(fmt.format(*r))
    sys.stdout.write("\n")

    if failures:
        sys.stdout.write("REGRESSIONS DETECTED:\n")
        for line in failures:
            sys.stdout.write(f"  * {line}\n")
        sys.stdout.write("\n")
        return 1

    sys.stdout.write("No regression. All metrics within tolerance.\n\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
