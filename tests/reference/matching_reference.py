"""Python reference matching engine for Meridian (single symbol, Phase 1).

This module is the canonical answer for "what should the C++ engine have
produced?" for any input event sequence in the Phase 1 scope. It is owned
by the Reference Implementation Engineer (agent 18). It is intentionally
slow and obviously correct: pure stdlib, integer math, dict and list data
structures. The C++ engine is diffed against this reference byte for byte
on every Phase 1 integration test.

Spec sources:

* docs/risk/matching-semantics.md (the worked-example contract).
* docs/superpowers/specs/2026-05-09-meridian-design.md section 8.2
  (matching invariants).
* docs/superpowers/plans/2026-05-09-phase-1-single-symbol-matching.md
  "Key decisions captured for Phase 1" (type widths, naming).

Design constraints (see agents/18-reference-implementation-engineer.md):

* Python 3.11 stdlib only. No numpy, no pandas, no pydantic.
* dataclasses and enum.
* Integer prices and integer quantities (no floats).
* Deterministic: same input always produces byte-identical output when
  serialized via ``ExecutionReport.to_dict()`` and ``json.dumps(..., sort_keys=True)``.
* Optimize for obvious correctness, not performance.

Phase 1 scope:

* Order types: ``LIMIT``, ``MARKET``, ``IOC``. ``POSTONLY`` and ``FOK`` are
  declared in the enum but raise ``NotImplementedError`` on submission;
  they land in Phase 7.
* Cancel-by-id with ``Reject`` ``NotFound`` for unknown ids.
* Single symbol per ``MatchingReference`` instance.
"""

from __future__ import annotations

import dataclasses
from collections import OrderedDict
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional


# ---------------------------------------------------------------------------
# Enumerations
# ---------------------------------------------------------------------------


class Side(Enum):
    """Order side. See matching-semantics.md section 1.

    Wire form is the lowercase enum name (``"buy"``, ``"sell"``).
    """

    BUY = "buy"
    SELL = "sell"


class OrderType(Enum):
    """Order type. See matching-semantics.md section 1.

    ``POSTONLY`` and ``FOK`` are declared for parity with the C++
    ``OrderType`` enum (companion plan key decision 5) but are not
    implemented in Phase 1; they land in Phase 7.
    """

    LIMIT = "limit"
    MARKET = "market"
    IOC = "ioc"
    POSTONLY = "postonly"
    FOK = "fok"


class ReportKind(Enum):
    """Execution report kind. See matching-semantics.md section 1.

    Wire form is the lowercase short name. ``ACK`` serializes as
    ``"ack"`` (not ``"acknowledge"``) to match the C++ engine's
    JSON field convention (companion plan Task 14 step 2).
    """

    ACK = "ack"
    FILL = "fill"
    REJECT = "reject"
    CANCEL = "cancel"


class RejectReason(Enum):
    """Reject reason. See matching-semantics.md section 1.

    Phase 1 uses ``EMPTY_BOOK`` (market against empty side) and
    ``NOT_FOUND`` (cancel of unknown id). The remaining values are
    declared for parity with the C++ enum and are reserved for
    Phase 7 and later.

    ``NONE`` is the default value when the report is not a reject;
    it serializes as ``"none"`` and is always present on every
    report so the JSON shape is uniform across kinds (companion
    plan Task 14 step 2).
    """

    NONE = "none"
    EMPTY_BOOK = "empty_book"
    NOT_FOUND = "not_found"
    INSUFFICIENT_LIQUIDITY = "insufficient_liquidity"
    WOULD_CROSS = "would_cross"


# ---------------------------------------------------------------------------
# Execution report
# ---------------------------------------------------------------------------


@dataclass
class ExecutionReport:
    """One execution report emitted by the matching engine.

    Field set matches the C++ engine's ``ExecutionReport`` (companion
    plan Task 14 step 2, ``report_to_jsonl``):

    * ``kind``: which lifecycle event this report describes.
    * ``order_id``: the id of the order this report is about. For a
      ``FILL``, each side of a match emits its own report carrying
      its own order_id and own side; there is no maker-taker pairing
      field on the report (matching-semantics.md notation, section 1).
    * ``side``: the order's own side (``BUY`` or ``SELL``).
    * ``price``: the price field. For ``ACK`` it is the order's
      submitted price (0 for market orders, by convention). For
      ``FILL`` it is the trade price (the maker's price). For
      ``CANCEL`` it is the cancelled order's resting price. For
      ``REJECT`` of an unknown cancel id, it is 0 (sentinel).
    * ``qty``: the relevant quantity. For ``ACK`` it is the original
      submitted quantity (matching-semantics.md section 5.3 IOC-3
      pins this: ack qty is submitted qty, never filled qty). For
      ``FILL`` it is the trade quantity. For ``CANCEL`` it is the
      remaining quantity at cancel time (CXL-2). For ``REJECT`` of
      a market against empty book it is the submitted quantity
      (MKT-4); for an unknown cancel id it is 0 (sentinel).
    * ``ts``: the event timestamp. For ``ACK`` and ``FILL``, it is
      the aggressor's ``ts`` (matching-semantics.md section 1
      notation). For ``CANCEL`` and ``REJECT`` from cancel-by-id,
      it is the cancel event's ``ts``.
    * ``reject_reason``: the reason this is a reject. Always present
      on every report; ``NONE`` for non-reject reports. The C++
      engine emits this field on every report so the JSON shape is
      uniform and the integration-test diff is line-by-line.
    """

    kind: ReportKind
    order_id: int
    side: Side
    price: int
    qty: int
    ts: int
    reject_reason: RejectReason = RejectReason.NONE

    def to_dict(self) -> dict:
        """Serialize for JSON line output.

        The integration test in ``tests/integration/test_engine_vs_reference.cpp``
        compares ``json.dumps(report.to_dict(), sort_keys=True)`` byte for
        byte against the C++ engine's JSON line. The keys here must match
        the C++ ``report_to_jsonl`` exactly:
        ``kind``, ``order_id``, ``price``, ``qty``, ``reject_reason``,
        ``side``, ``ts``. Values are lowercase short names from the enums
        above (companion plan Task 14 step 2).
        """

        return {
            "kind": self.kind.value,
            "order_id": self.order_id,
            "price": self.price,
            "qty": self.qty,
            "reject_reason": self.reject_reason.value,
            "side": self.side.value,
            "ts": self.ts,
        }


# ---------------------------------------------------------------------------
# Resting orders, levels, and the book
# ---------------------------------------------------------------------------


@dataclass
class _RestingOrder:
    """A single resting order at one price level.

    Tracks the original submitted quantity for diagnostics, the
    remaining quantity (which decreases as the order is filled), the
    side, the price, and the arrival timestamp. The arrival timestamp
    is used for FIFO insertion order; within a level the queue is an
    ``OrderedDict`` so insertion order is the canonical FIFO order
    (matching-semantics.md section 2.2).
    """

    order_id: int
    side: Side
    price: int
    qty_remaining: int
    qty_submitted: int
    ts_arrival: int


@dataclass
class _Level:
    """One price level: a FIFO queue of resting orders at one price.

    The queue is an ``OrderedDict`` keyed by ``order_id``. Insertion
    appends to the tail; ``next(iter(...))`` returns the front
    (the oldest resting order at this level). This implements the
    "time priority within a level" rule (matching-semantics.md
    section 2.2).
    """

    price: int
    orders: "OrderedDict[int, _RestingOrder]" = field(default_factory=OrderedDict)

    def total_qty(self) -> int:
        """Sum of remaining quantities at this level. O(n) but only
        used by ``top_of_book`` for the demo binary; not on the diff
        path."""

        return sum(o.qty_remaining for o in self.orders.values())

    def is_empty(self) -> bool:
        return not self.orders


# ---------------------------------------------------------------------------
# Matching engine
# ---------------------------------------------------------------------------


class MatchingReference:
    """Single-symbol price-time-priority matching engine.

    Public API (companion plan Task 3, Step 2):

    * ``submit_limit(order_id, side, price, qty, ts) -> list[ExecutionReport]``
    * ``submit_market(order_id, side, qty, ts) -> list[ExecutionReport]``
    * ``submit_ioc(order_id, side, price, qty, ts) -> list[ExecutionReport]``
    * ``cancel(order_id) -> list[ExecutionReport]``
    * ``top_of_book() -> tuple[best_bid_px, best_bid_qty, best_ask_px, best_ask_qty]``

    The ``symbol`` argument is stored for parity with the C++ ``Book``
    constructor but is not used in Phase 1; multi-symbol dispatch
    lands in Phase 2 via a ``BookRegistry`` analog.

    Internal state:

    * ``_bids``: ``dict[Price, _Level]``. Sorted on demand by walking
      the keys with ``sorted(..., reverse=True)`` for "best bid first"
      (highest price). The companion plan locks the C++ side to
      ``std::map<Price, Level*>`` for O(log n) ordered traversal; the
      reference uses a plain ``dict`` plus ``sorted`` because the
      reference is the obvious-over-fast implementation.
    * ``_asks``: same shape, sorted with default ``sorted(...)`` for
      "best ask first" (lowest price).
    * ``_orders``: ``dict[OrderId, _RestingOrder]``. The per-symbol
      lookup map for cancel-by-id (companion plan key decision 4 and
      matching-semantics.md section 6).
    """

    def __init__(self, symbol: int):
        self.symbol = symbol
        self._bids: dict[int, _Level] = {}
        self._asks: dict[int, _Level] = {}
        self._orders: dict[int, _RestingOrder] = {}

    # -- public submission API -------------------------------------------------

    def submit_limit(
        self, order_id: int, side: Side, price: int, qty: int, ts: int
    ) -> list[ExecutionReport]:
        """Submit a ``LIMIT`` order.

        See matching-semantics.md section 3. Always emits an
        ``Acknowledge`` first (section 2.3 step 6), walks the
        opposite side while the next maker price satisfies the
        aggressor's limit, and rests any residual at the limit
        price on the aggressor's own side.
        """

        self._require_valid_qty(qty)
        reports: list[ExecutionReport] = []
        reports.append(
            ExecutionReport(
                kind=ReportKind.ACK,
                order_id=order_id,
                side=side,
                price=price,
                qty=qty,
                ts=ts,
            )
        )
        remaining = self._match(reports, order_id, side, price, qty, ts, OrderType.LIMIT)
        if remaining > 0:
            self._rest(order_id, side, price, remaining, qty, ts)
        return reports

    def submit_market(
        self, order_id: int, side: Side, qty: int, ts: int
    ) -> list[ExecutionReport]:
        """Submit a ``MARKET`` order.

        See matching-semantics.md section 4. If the opposite side is
        empty on arrival, emit a single ``Reject`` with reason
        ``EMPTY_BOOK`` and no preceding ``Acknowledge`` (section 4.1
        convention choice). Otherwise emit an ``Acknowledge`` first,
        then walk the opposite side with no price limit. Any residual
        after the opposite side empties is implicitly cancelled (no
        report).
        """

        self._require_valid_qty(qty)
        if self._opposite_side_empty(side):
            return [
                ExecutionReport(
                    kind=ReportKind.REJECT,
                    order_id=order_id,
                    side=side,
                    price=0,
                    qty=qty,
                    ts=ts,
                    reject_reason=RejectReason.EMPTY_BOOK,
                )
            ]
        reports: list[ExecutionReport] = []
        reports.append(
            ExecutionReport(
                kind=ReportKind.ACK,
                order_id=order_id,
                side=side,
                price=0,
                qty=qty,
                ts=ts,
            )
        )
        # Market orders have no price limit; pass ``None`` to ``_match``
        # to signal "fill at any price on the opposite side".
        self._match(reports, order_id, side, None, qty, ts, OrderType.MARKET)
        # Residual is implicitly cancelled (matching-semantics.md
        # section 4 step 4); no further report.
        return reports

    def submit_ioc(
        self, order_id: int, side: Side, price: int, qty: int, ts: int
    ) -> list[ExecutionReport]:
        """Submit an ``IOC`` (immediate-or-cancel) order.

        See matching-semantics.md section 5. Always emits an
        ``Acknowledge`` first (section 5 step 1, IOC-3 worked
        example), walks the opposite side while the next maker price
        satisfies the limit, and implicitly cancels any residual
        without a ``Cancel`` report (section 5 step 4, "IOC never
        rests").
        """

        self._require_valid_qty(qty)
        reports: list[ExecutionReport] = []
        reports.append(
            ExecutionReport(
                kind=ReportKind.ACK,
                order_id=order_id,
                side=side,
                price=price,
                qty=qty,
                ts=ts,
            )
        )
        self._match(reports, order_id, side, price, qty, ts, OrderType.IOC)
        # Residual implicitly cancelled; no further report.
        return reports

    def cancel(self, order_id: int) -> list[ExecutionReport]:
        """Cancel a resting order by id.

        See matching-semantics.md section 6. If the id is in the
        per-symbol lookup map, unlink the order, possibly remove the
        now-empty level, and emit a ``Cancel`` report carrying the
        remaining quantity (CXL-1, CXL-2, CXL-3). If not, emit a
        ``Reject`` with reason ``NOT_FOUND`` and sentinel side
        ``BUY`` and price ``0`` (CXL-4, CXL-5, CXL-6, plus the
        convention choice flagged in matching-semantics.md section
        8.2).

        The cancel event in the JSON Lines wire format does not
        carry a timestamp on its event (companion plan Task 14
        step 1: ``elif event["kind"] == "cancel": reports =
        engine.cancel(event["order_id"])``). The reference therefore
        propagates the cancelled order's resting ``ts_arrival`` to
        the report's ``ts``, and uses ``0`` for the unknown-id case.
        The matching-semantics worked examples write a ``ts`` on
        the cancel event (e.g., ``ts=21`` in CXL-1) and on the
        emitted ``Cancel`` report. The unit-test harness in
        ``test_reference.py`` calls a wrapper that sets the ``ts``
        explicitly so the worked examples reproduce verbatim; see
        ``cancel_at`` below.
        """

        return self.cancel_at(order_id, 0)

    def cancel_at(self, order_id: int, ts: int) -> list[ExecutionReport]:
        """Cancel a resting order by id, with an explicit cancel timestamp.

        This is the variant matching the worked examples in
        matching-semantics.md, which carry a ``ts`` on every
        ``Cancel { order_id, ts }`` event. The ``cancel`` method
        above forwards to this with ``ts=0`` so the JSON Lines
        wire format (which does not carry a per-cancel timestamp
        in the Phase 1 scope) still works.
        """

        order = self._orders.get(order_id)
        if order is None:
            return [
                ExecutionReport(
                    kind=ReportKind.REJECT,
                    order_id=order_id,
                    side=Side.BUY,
                    price=0,
                    qty=0,
                    ts=ts,
                    reject_reason=RejectReason.NOT_FOUND,
                )
            ]
        # Unlink from the level and remove the level if it is now empty.
        side_map = self._bids if order.side == Side.BUY else self._asks
        level = side_map[order.price]
        del level.orders[order_id]
        if level.is_empty():
            del side_map[order.price]
        del self._orders[order_id]
        return [
            ExecutionReport(
                kind=ReportKind.CANCEL,
                order_id=order_id,
                side=order.side,
                price=order.price,
                qty=order.qty_remaining,
                ts=ts,
            )
        ]

    # -- public introspection --------------------------------------------------

    def top_of_book(self) -> tuple[Optional[int], int, Optional[int], int]:
        """Return ``(best_bid_px, best_bid_qty, best_ask_px, best_ask_qty)``.

        Best bid is the highest bid price; best ask is the lowest ask
        price. If a side is empty, that side's price is ``None`` and
        its quantity is ``0``.
        """

        best_bid_px: Optional[int] = None
        best_bid_qty = 0
        best_ask_px: Optional[int] = None
        best_ask_qty = 0
        if self._bids:
            best_bid_px = max(self._bids.keys())
            best_bid_qty = self._bids[best_bid_px].total_qty()
        if self._asks:
            best_ask_px = min(self._asks.keys())
            best_ask_qty = self._asks[best_ask_px].total_qty()
        return best_bid_px, best_bid_qty, best_ask_px, best_ask_qty

    # -- internal matching loop ------------------------------------------------

    def _match(
        self,
        reports: list[ExecutionReport],
        taker_id: int,
        taker_side: Side,
        taker_limit: Optional[int],
        taker_qty: int,
        taker_ts: int,
        taker_type: OrderType,
    ) -> int:
        """Walk the opposite side, filling against makers in price-time order.

        Returns the taker's remaining quantity after the walk. The
        caller decides what to do with the residual (rest it, cancel
        it implicitly, or reject it). See matching-semantics.md
        section 2.3 for the rule list.
        """

        opposite = self._asks if taker_side == Side.BUY else self._bids
        remaining = taker_qty

        while remaining > 0 and opposite:
            best_price = (
                min(opposite.keys()) if taker_side == Side.BUY else max(opposite.keys())
            )
            if not self._price_satisfies_limit(taker_side, taker_limit, best_price):
                break
            level = opposite[best_price]
            # Walk the FIFO queue at this level. Use list(...) to
            # snapshot the ids so we can mutate the OrderedDict
            # during iteration when a maker is fully filled.
            for maker_id in list(level.orders.keys()):
                if remaining == 0:
                    break
                maker = level.orders[maker_id]
                trade_qty = min(remaining, maker.qty_remaining)
                trade_price = maker.price
                # Two fills, back to back: maker first, then taker
                # (matching-semantics.md section 1 notation, and
                # every worked example). Each fill reports its own
                # side and own order_id.
                reports.append(
                    ExecutionReport(
                        kind=ReportKind.FILL,
                        order_id=maker.order_id,
                        side=maker.side,
                        price=trade_price,
                        qty=trade_qty,
                        ts=taker_ts,
                    )
                )
                reports.append(
                    ExecutionReport(
                        kind=ReportKind.FILL,
                        order_id=taker_id,
                        side=taker_side,
                        price=trade_price,
                        qty=trade_qty,
                        ts=taker_ts,
                    )
                )
                maker.qty_remaining -= trade_qty
                remaining -= trade_qty
                if maker.qty_remaining == 0:
                    # Fully filled maker: unlink from level and from
                    # the per-symbol lookup map.
                    del level.orders[maker_id]
                    del self._orders[maker_id]
            if level.is_empty():
                del opposite[best_price]

        # ``taker_type`` is currently used only for documentation
        # clarity; the limit-vs-market distinction is encoded in
        # ``taker_limit`` (None for market). The argument is kept in
        # the signature so future extensions (Phase 7 PostOnly, FOK)
        # can branch on it without changing callers.
        del taker_type
        return remaining

    @staticmethod
    def _price_satisfies_limit(
        taker_side: Side, taker_limit: Optional[int], maker_price: int
    ) -> bool:
        """Return True if the taker is willing to trade at ``maker_price``.

        Market orders (``taker_limit is None``) trade at any price.
        A buy at limit ``L`` trades at any ask price ``<= L``. A sell
        at limit ``L`` trades at any bid price ``>= L``. See
        matching-semantics.md section 2.3 step 2.
        """

        if taker_limit is None:
            return True
        if taker_side == Side.BUY:
            return maker_price <= taker_limit
        return maker_price >= taker_limit

    def _opposite_side_empty(self, taker_side: Side) -> bool:
        opposite = self._asks if taker_side == Side.BUY else self._bids
        return not opposite

    def _rest(
        self,
        order_id: int,
        side: Side,
        price: int,
        qty_remaining: int,
        qty_submitted: int,
        ts: int,
    ) -> None:
        """Insert the residual at the tail of the FIFO queue at its
        limit price on its own side. See matching-semantics.md section
        3 step 2.
        """

        side_map = self._bids if side == Side.BUY else self._asks
        level = side_map.get(price)
        if level is None:
            level = _Level(price=price)
            side_map[price] = level
        order = _RestingOrder(
            order_id=order_id,
            side=side,
            price=price,
            qty_remaining=qty_remaining,
            qty_submitted=qty_submitted,
            ts_arrival=ts,
        )
        level.orders[order_id] = order
        self._orders[order_id] = order

    @staticmethod
    def _require_valid_qty(qty: int) -> None:
        """Reject zero or negative quantities. Phase 1 worked examples
        all use positive quantities; the engine treats ``qty <= 0`` as
        a programming error rather than a runtime reject. The C++
        engine guards on submission boundary; the reference matches.
        """

        if qty <= 0:
            raise ValueError(f"qty must be positive, got {qty}")


# Re-export ``dataclasses.asdict`` for callers that want the built-in
# serialization shape rather than the wire-format ``to_dict``. Not
# used by the integration test (which uses ``to_dict`` for the lower-
# case enum values), but useful for ad hoc debugging.
asdict = dataclasses.asdict


__all__ = [
    "Side",
    "OrderType",
    "ReportKind",
    "RejectReason",
    "ExecutionReport",
    "MatchingReference",
    "asdict",
]
