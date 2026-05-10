"""Python reference matching engine for Meridian (multi-symbol).

This module is the canonical answer for "what should the C++ engine have
produced?" for any input event sequence. It is intentionally slow and
obviously correct: pure stdlib, integer math, dict and list data
structures. The C++ engine is diffed against this reference byte for
byte on every integration test.

Source: docs/risk/matching-semantics.md (the worked-example contract).

Design constraints:

* Python 3.11 stdlib only. No numpy, no pandas, no pydantic.
* dataclasses and enum.
* Integer prices and integer quantities (no floats).
* Deterministic: same input always produces byte-identical output when
  serialized via ``ExecutionReport.to_dict()`` and ``json.dumps(..., sort_keys=True)``.
* Optimize for obvious correctness, not performance.

Single-symbol scope:

* Order types: ``LIMIT``, ``MARKET``, ``IOC``. ``POSTONLY`` and ``FOK`` are
  declared in the enum but raise ``NotImplementedError`` on submission;
  they land alongside the post-only / FOK milestone.
* Cancel-by-id with ``Reject`` ``NotFound`` for unknown ids.
* Single symbol per ``MatchingReference`` instance (legacy form,
  ``MatchingReference(symbol=1)``).

Multi-symbol additions:

* Multi-symbol dispatch. ``MatchingReference(symbols=[1, 2, 3, 4, 5])``
  registers a fixed set of symbols at construction; ``submit_*`` accept
  an optional ``symbol`` keyword to select which book receives the order.
* Cross-symbol cancel-by-id. ``cancel(order_id)`` and ``cancel_at(order_id, ts)``
  consult a top-level id-to-symbol map and dispatch to the correct
  per-symbol book. A cancel for an order on symbol X never affects book Y.
* Unknown-symbol reject. A ``NewOrder`` for a symbol that is not in the
  registered set emits a single ``Reject`` with reason ``UNKNOWN_SYMBOL``
  (no preceding ``Acknowledge``), matching the pattern used for
  ``EMPTY_BOOK`` rejects on market-against-empty.
"""

from __future__ import annotations

import dataclasses
from collections import OrderedDict
from dataclasses import dataclass, field
from enum import Enum
from typing import Iterable, Optional


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
    ``OrderType`` enum but are not implemented yet; they land alongside
    the post-only / FOK milestone.
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
    JSON field convention.
    """

    ACK = "ack"
    FILL = "fill"
    REJECT = "reject"
    CANCEL = "cancel"


class RejectReason(Enum):
    """Reject reason. See matching-semantics.md section 1.

    Single-symbol matching uses ``EMPTY_BOOK`` (market against empty
    side) and ``NOT_FOUND`` (cancel of unknown id). The multi-symbol
    extension adds ``UNKNOWN_SYMBOL`` (NewOrder against an
    unregistered symbol). The remaining values are declared for
    parity with the C++ enum and are reserved for the post-only / FOK
    work and beyond.

    ``NONE`` is the default value when the report is not a reject;
    it serializes as ``"none"`` and is always present on every
    report so the JSON shape is uniform across kinds.

    Wire-format mapping (must match C++ ``RejectReason``):

    * ``NONE`` -> ``"none"``
    * ``EMPTY_BOOK`` -> ``"empty_book"`` (C++ ``RejectReason::EmptyBook = 1``)
    * ``NOT_FOUND`` -> ``"not_found"`` (C++ ``RejectReason::NotFound = 2``)
    * ``INSUFFICIENT_LIQUIDITY`` -> ``"insufficient_liquidity"``
      (reserved, C++ id 3)
    * ``WOULD_CROSS`` -> ``"would_cross"`` (reserved, C++ id 4)
    * ``UNKNOWN_SYMBOL`` -> ``"unknown_symbol"``
      (C++ ``RejectReason::UnknownSymbol = 5``)
    """

    NONE = "none"
    EMPTY_BOOK = "empty_book"
    NOT_FOUND = "not_found"
    INSUFFICIENT_LIQUIDITY = "insufficient_liquidity"
    WOULD_CROSS = "would_cross"
    UNKNOWN_SYMBOL = "unknown_symbol"


# ---------------------------------------------------------------------------
# Execution report
# ---------------------------------------------------------------------------


@dataclass
class ExecutionReport:
    """One execution report emitted by the matching engine.

    Field set matches the C++ engine's ``ExecutionReport`` (see
    ``report_to_jsonl`` in src/):

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
        above.
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
# Resting orders, levels, and the per-symbol book
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


class _SingleSymbolBook:
    """Single-symbol price-time-priority matching engine.

    This is the per-symbol matching primitive. The public
    ``MatchingReference`` class owns one of these per registered symbol
    and routes new-order and cancel events to the correct book. The
    single-symbol worked examples in matching-semantics.md operate at
    this layer (one book, one symbol).

    Internal state mirrors the C++ ``Book``:

    * ``_bids``: ``dict[Price, _Level]``, sorted on demand by walking
      the keys with ``sorted(..., reverse=True)`` for "best bid first"
      (highest price). The C++ side uses ``std::map<Price, Level*>``
      for O(log n) ordered traversal; the reference uses a plain
      ``dict`` plus ``sorted`` because it is the obvious-over-fast
      implementation.
    * ``_asks``: same shape, sorted with default ``sorted(...)`` for
      "best ask first" (lowest price).
    * ``_orders``: ``dict[OrderId, _RestingOrder]``. The per-symbol
      lookup map for cancel-by-id (matching-semantics.md section 8).
      In multi-symbol mode there is also a cross-symbol id-to-symbol
      map at the ``MatchingReference`` level; the per-symbol map
      remains because the matching loop already uses it for
      fully-filled-maker cleanup (the dual-index choice is documented
      in ADR 0002).
    """

    def __init__(self, symbol: int):
        self.symbol = symbol
        self._bids: dict[int, _Level] = {}
        self._asks: dict[int, _Level] = {}
        self._orders: dict[int, _RestingOrder] = {}

    # -- submission API (called from ``MatchingReference``) -------------------

    def submit_limit(
        self, order_id: int, side: Side, price: int, qty: int, ts: int
    ) -> tuple[list[ExecutionReport], list[int]]:
        """Submit a ``LIMIT`` order to this book.

        Returns ``(reports, fully_filled_maker_ids)``. The second tuple
        element is the list of maker order ids that were fully consumed
        during this match and must be removed from the cross-symbol
        id-to-symbol map by the caller. The third tuple element implicit
        in this contract is "did the taker rest?", surfaced by checking
        whether ``order_id`` is in ``self._orders`` after the call.

        See matching-semantics.md section 3.
        """

        _require_valid_qty(qty)
        reports: list[ExecutionReport] = [
            ExecutionReport(
                kind=ReportKind.ACK,
                order_id=order_id,
                side=side,
                price=price,
                qty=qty,
                ts=ts,
            )
        ]
        fully_filled: list[int] = []
        remaining = self._match(
            reports, fully_filled, order_id, side, price, qty, ts, OrderType.LIMIT
        )
        if remaining > 0:
            self._rest(order_id, side, price, remaining, qty, ts)
        return reports, fully_filled

    def submit_market(
        self, order_id: int, side: Side, qty: int, ts: int
    ) -> tuple[list[ExecutionReport], list[int]]:
        """Submit a ``MARKET`` order to this book.

        See matching-semantics.md section 4. If the opposite side is
        empty on arrival, emit a single ``Reject EMPTY_BOOK`` and no
        preceding ``Acknowledge``.
        """

        _require_valid_qty(qty)
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
            ], []
        reports: list[ExecutionReport] = [
            ExecutionReport(
                kind=ReportKind.ACK,
                order_id=order_id,
                side=side,
                price=0,
                qty=qty,
                ts=ts,
            )
        ]
        fully_filled: list[int] = []
        # Market orders have no price limit; pass ``None`` to ``_match``
        # to signal "fill at any price on the opposite side".
        self._match(
            reports, fully_filled, order_id, side, None, qty, ts, OrderType.MARKET
        )
        # Residual is implicitly cancelled (matching-semantics.md
        # section 4 step 4); no further report.
        return reports, fully_filled

    def submit_ioc(
        self, order_id: int, side: Side, price: int, qty: int, ts: int
    ) -> tuple[list[ExecutionReport], list[int]]:
        """Submit an ``IOC`` (immediate-or-cancel) order to this book.

        See matching-semantics.md section 5.
        """

        _require_valid_qty(qty)
        reports: list[ExecutionReport] = [
            ExecutionReport(
                kind=ReportKind.ACK,
                order_id=order_id,
                side=side,
                price=price,
                qty=qty,
                ts=ts,
            )
        ]
        fully_filled: list[int] = []
        self._match(
            reports, fully_filled, order_id, side, price, qty, ts, OrderType.IOC
        )
        # Residual implicitly cancelled; no further report.
        return reports, fully_filled

    def submit_post_only(
        self, order_id: int, side: Side, price: int, qty: int, ts: int
    ) -> tuple[list[ExecutionReport], list[int]]:
        """Submit a ``POSTONLY`` (post-only) order to this book.

        See matching-semantics.md section 6. A post-only order must
        rest; if on arrival it would cross (best ask <= buy limit, or
        best bid >= sell limit), the engine emits a single ``Reject``
        with reason ``WOULD_CROSS`` and no preceding ``Acknowledge``.
        Otherwise it acks and rests like a non-crossing limit.
        """

        _require_valid_qty(qty)
        opposite = self._asks if side == Side.BUY else self._bids
        if opposite:
            best = (
                min(opposite.keys()) if side == Side.BUY else max(opposite.keys())
            )
            if _price_satisfies_limit(side, price, best):
                return [
                    ExecutionReport(
                        kind=ReportKind.REJECT,
                        order_id=order_id,
                        side=side,
                        price=price,
                        qty=qty,
                        ts=ts,
                        reject_reason=RejectReason.WOULD_CROSS,
                    )
                ], []
        # No cross. Ack and rest. The matching loop is bypassed
        # entirely; nothing to sweep against by construction.
        reports: list[ExecutionReport] = [
            ExecutionReport(
                kind=ReportKind.ACK,
                order_id=order_id,
                side=side,
                price=price,
                qty=qty,
                ts=ts,
            )
        ]
        self._rest(order_id, side, price, qty, qty, ts)
        return reports, []

    def submit_fok(
        self, order_id: int, side: Side, price: int, qty: int, ts: int
    ) -> tuple[list[ExecutionReport], list[int]]:
        """Submit a ``FOK`` (fill-or-kill) order to this book.

        See matching-semantics.md section 7. Inspects opposite-side
        liquidity at acceptable prices. If less than ``qty`` is
        available, emits a single ``Reject`` with reason
        ``INSUFFICIENT_LIQUIDITY`` and no preceding ``Acknowledge``.
        Otherwise acks and fills in full via the matching loop.
        """

        _require_valid_qty(qty)
        opposite = self._asks if side == Side.BUY else self._bids
        sorted_prices = (
            sorted(opposite.keys())
            if side == Side.BUY
            else sorted(opposite.keys(), reverse=True)
        )
        available = 0
        for p in sorted_prices:
            if not _price_satisfies_limit(side, price, p):
                break
            available += opposite[p].total_qty()
            if available >= qty:
                break
        if available < qty:
            return [
                ExecutionReport(
                    kind=ReportKind.REJECT,
                    order_id=order_id,
                    side=side,
                    price=price,
                    qty=qty,
                    ts=ts,
                    reject_reason=RejectReason.INSUFFICIENT_LIQUIDITY,
                )
            ], []
        # Sufficient liquidity. Ack and fully fill via the matching
        # loop. FOK never rests: by construction the sweep consumes
        # exactly ``qty`` here and leaves zero residual.
        reports: list[ExecutionReport] = [
            ExecutionReport(
                kind=ReportKind.ACK,
                order_id=order_id,
                side=side,
                price=price,
                qty=qty,
                ts=ts,
            )
        ]
        fully_filled: list[int] = []
        self._match(
            reports, fully_filled, order_id, side, price, qty, ts, OrderType.FOK
        )
        return reports, fully_filled

    def cancel_at(
        self, order_id: int, ts: int
    ) -> list[ExecutionReport]:
        """Cancel a resting order on this book by id.

        Caller is responsible for confirming via the cross-symbol
        id-to-symbol map that this book actually owns ``order_id``
        before calling. If the id is not in this book's per-symbol
        ``_orders`` map (which should never happen when the caller
        routes correctly), the method emits the same
        ``Reject NOT_FOUND`` shape as the public API for safety.

        See matching-semantics.md section 8.
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

    # -- introspection --------------------------------------------------------

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

    # -- internal matching loop -----------------------------------------------

    def _match(
        self,
        reports: list[ExecutionReport],
        fully_filled: list[int],
        taker_id: int,
        taker_side: Side,
        taker_limit: Optional[int],
        taker_qty: int,
        taker_ts: int,
        taker_type: OrderType,
    ) -> int:
        """Walk the opposite side, filling against makers in price-time order.

        Returns the taker's remaining quantity after the walk. Appends
        each fully-filled maker's id to ``fully_filled`` so the caller
        can remove it from the cross-symbol id-to-symbol map. See
        matching-semantics.md section 2.3 for the rule list.
        """

        opposite = self._asks if taker_side == Side.BUY else self._bids
        remaining = taker_qty

        while remaining > 0 and opposite:
            best_price = (
                min(opposite.keys()) if taker_side == Side.BUY else max(opposite.keys())
            )
            if not _price_satisfies_limit(taker_side, taker_limit, best_price):
                break
            level = opposite[best_price]
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
                    del level.orders[maker_id]
                    del self._orders[maker_id]
                    fully_filled.append(maker_id)
            if level.is_empty():
                del opposite[best_price]

        # ``taker_type`` is currently used only for documentation
        # clarity; the limit-vs-market distinction is encoded in
        # ``taker_limit`` (None for market). PostOnly and FOK will
        # branch on it without changing callers.
        del taker_type
        return remaining

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
        3 step 2."""

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


# ---------------------------------------------------------------------------
# Module-level helpers (shared by the per-symbol book and the dispatcher).
# ---------------------------------------------------------------------------


def _price_satisfies_limit(
    taker_side: Side, taker_limit: Optional[int], maker_price: int
) -> bool:
    """Return True if the taker is willing to trade at ``maker_price``.

    Market orders (``taker_limit is None``) trade at any price. A buy
    at limit ``L`` trades at any ask price ``<= L``. A sell at limit
    ``L`` trades at any bid price ``>= L``. See matching-semantics.md
    section 2.3 step 2.
    """

    if taker_limit is None:
        return True
    if taker_side == Side.BUY:
        return maker_price <= taker_limit
    return maker_price >= taker_limit


def _require_valid_qty(qty: int) -> None:
    """Reject zero or negative quantities. The worked examples all use
    positive quantities; the engine treats ``qty <= 0`` as a
    programming error rather than a runtime reject. The C++ engine
    guards on submission boundary; the reference matches.
    """

    if qty <= 0:
        raise ValueError(f"qty must be positive, got {qty}")


# ---------------------------------------------------------------------------
# Multi-symbol matching reference (public API)
# ---------------------------------------------------------------------------


class MatchingReference:
    """Multi-symbol price-time-priority matching engine.

    Public API:

    * Single-symbol mode (legacy, backward compatible)::

          engine = MatchingReference(symbol=1)
          engine.submit_limit(101, Side.BUY, 10000, 50, 1)
          engine.cancel_at(101, ts=21)

      The ``symbol`` keyword on ``submit_*`` defaults to the single
      registered symbol when only one is configured, so every legacy
      call site continues to work unchanged.

    * Multi-symbol mode::

          engine = MatchingReference(symbols=[1, 2, 3, 4, 5])
          engine.submit_limit(101, Side.BUY, 10000, 50, 1, symbol=1)
          engine.submit_limit(201, Side.BUY, 20000, 30, 2, symbol=2)
          engine.cancel_at(101, ts=21)  # cross-symbol; routes via id map

      ``cancel`` and ``cancel_at`` route to the correct per-symbol book
      via an internal id-to-symbol map. A cancel for an order that was
      placed on symbol X never affects symbol Y (the dual-index design
      is documented in ADR 0002).

    * Unknown-symbol reject::

          engine.submit_limit(999, Side.BUY, 10000, 50, 1, symbol=99)
          # -> [Reject UNKNOWN_SYMBOL]

      A ``NewOrder`` whose ``symbol`` is not in the registered set
      produces a single ``Reject UNKNOWN_SYMBOL`` with no preceding
      ``Acknowledge`` (mirrors the EMPTY_BOOK shape from
      matching-semantics.md section 4).

    Constructor forms:

    * ``MatchingReference(symbol=1)``: legacy single-symbol form.
      Registers exactly the one symbol given.
    * ``MatchingReference(symbols=[1, 2, 3])``: multi-symbol form.
      Registers each symbol in the iterable. Order of the iterable
      does not matter; books are keyed by symbol id.
    * ``MatchingReference()``: empty registry. Every ``submit_*``
      will produce ``Reject UNKNOWN_SYMBOL``. Useful only for tests
      of the unknown-symbol path.

    Internal state:

    * ``_books``: ``dict[Symbol, _SingleSymbolBook]``. One per
      registered symbol; created at construction.
    * ``_id_to_symbol``: ``dict[OrderId, Symbol]``. The cross-symbol
      lookup that the cancel path consults to find which book owns
      a given order id. Updated on every successful rest (insert),
      every fully-filled-maker cleanup (remove), and every cancel
      (remove). The C++ side calls this ``OrderIndex``; the reference
      uses a single dict for obvious correctness.
    """

    def __init__(
        self,
        *,
        symbol: Optional[int] = None,
        symbols: Optional[Iterable[int]] = None,
    ):
        if symbol is not None and symbols is not None:
            raise ValueError("pass either ``symbol=`` or ``symbols=``, not both")
        if symbol is not None:
            registered = [int(symbol)]
        elif symbols is not None:
            registered = [int(s) for s in symbols]
        else:
            registered = []
        # Preserve the publicly documented attribute ``symbol`` for
        # single-symbol callers that read it. In multi-symbol mode it
        # is ``None``; callers should use ``symbols`` instead.
        if len(registered) == 1:
            self.symbol: Optional[int] = registered[0]
        else:
            self.symbol = None
        self.symbols: tuple[int, ...] = tuple(registered)
        self._books: dict[int, _SingleSymbolBook] = {
            s: _SingleSymbolBook(symbol=s) for s in registered
        }
        self._id_to_symbol: dict[int, int] = {}

    # -- submission API -------------------------------------------------------

    def submit_limit(
        self,
        order_id: int,
        side: Side,
        price: int,
        qty: int,
        ts: int,
        *,
        symbol: Optional[int] = None,
    ) -> list[ExecutionReport]:
        """Submit a ``LIMIT`` order. See matching-semantics.md section 3."""

        return self._route_new_order(
            "limit", order_id, side, price, qty, ts, symbol
        )

    def submit_market(
        self,
        order_id: int,
        side: Side,
        qty: int,
        ts: int,
        *,
        symbol: Optional[int] = None,
    ) -> list[ExecutionReport]:
        """Submit a ``MARKET`` order. See matching-semantics.md section 4.

        ``price`` is implicitly ``0`` (no limit). The unknown-symbol
        reject path emits ``price=0`` in the ``Reject``, matching the
        ``Acknowledge`` price for market orders.
        """

        return self._route_new_order(
            "market", order_id, side, 0, qty, ts, symbol
        )

    def submit_ioc(
        self,
        order_id: int,
        side: Side,
        price: int,
        qty: int,
        ts: int,
        *,
        symbol: Optional[int] = None,
    ) -> list[ExecutionReport]:
        """Submit an ``IOC`` order. See matching-semantics.md section 5."""

        return self._route_new_order(
            "ioc", order_id, side, price, qty, ts, symbol
        )

    def submit_post_only(
        self,
        order_id: int,
        side: Side,
        price: int,
        qty: int,
        ts: int,
        *,
        symbol: Optional[int] = None,
    ) -> list[ExecutionReport]:
        """Submit a ``POSTONLY`` order. See matching-semantics.md section 6."""

        return self._route_new_order(
            "postonly", order_id, side, price, qty, ts, symbol
        )

    def submit_fok(
        self,
        order_id: int,
        side: Side,
        price: int,
        qty: int,
        ts: int,
        *,
        symbol: Optional[int] = None,
    ) -> list[ExecutionReport]:
        """Submit a ``FOK`` order. See matching-semantics.md section 7."""

        return self._route_new_order(
            "fok", order_id, side, price, qty, ts, symbol
        )

    def cancel(self, order_id: int) -> list[ExecutionReport]:
        """Cancel a resting order by id. See matching-semantics.md section 6.

        Cross-symbol: looks up which book owns ``order_id`` via the
        internal id-to-symbol map; routes to that book's cancel.
        Unknown ids produce ``Reject NOT_FOUND``. Equivalent to
        ``cancel_at(order_id, ts=0)``; preserved for backward
        compatibility with the legacy single-symbol wire-format driver.
        """

        return self.cancel_at(order_id, 0)

    def cancel_at(self, order_id: int, ts: int) -> list[ExecutionReport]:
        """Cancel a resting order by id, with an explicit cancel timestamp.

        Cross-symbol routing: consults ``_id_to_symbol`` to find which
        book owns ``order_id``, then forwards to that book's
        ``cancel_at``. If the id is unknown across all books, emits a
        single ``Reject NOT_FOUND`` with sentinel side ``BUY`` and price
        ``0`` (matching-semantics.md section 8.2).
        """

        sym = self._id_to_symbol.get(order_id)
        if sym is None:
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
        reports = self._books[sym].cancel_at(order_id, ts)
        # On a successful cancel the per-symbol book has already removed
        # the id from its own ``_orders`` map. Mirror the removal in the
        # cross-symbol map.
        if reports and reports[0].kind == ReportKind.CANCEL:
            del self._id_to_symbol[order_id]
        return reports

    # -- introspection --------------------------------------------------------

    def top_of_book(
        self, symbol: Optional[int] = None
    ) -> tuple[Optional[int], int, Optional[int], int]:
        """Return ``(best_bid_px, best_bid_qty, best_ask_px, best_ask_qty)``
        for one symbol.

        Single-symbol callers can omit ``symbol`` and the engine returns
        the only registered book's top-of-book; this preserves the
        legacy call shapes. Multi-symbol callers must pass ``symbol``.
        """

        sym = self._resolve_symbol(symbol)
        return self._books[sym].top_of_book()

    def book(self, symbol: int) -> _SingleSymbolBook:
        """Return the per-symbol book for ``symbol``. Raises
        ``KeyError`` if ``symbol`` is not registered.

        Reserved for tests that want to assert per-symbol book state
        directly (e.g., "symbol 2's book is unchanged after symbol 1
        cancels"). Not used on the wire-format diff path.
        """

        return self._books[symbol]

    @property
    def _orders(self) -> dict[int, _RestingOrder]:
        """Backward-compatible flat view across all per-symbol books.

        The conservation-law tests in test_reference.py read
        ``engine._orders`` to assert that an aggressor (market/IOC)
        does not rest. In multi-symbol mode this property aggregates
        the per-symbol books' ``_orders`` maps so the existing tests
        keep passing without modification.
        """

        merged: dict[int, _RestingOrder] = {}
        for book in self._books.values():
            merged.update(book._orders)
        return merged

    # -- internals ------------------------------------------------------------

    def _resolve_symbol(self, symbol: Optional[int]) -> int:
        """Resolve an optional ``symbol`` argument to a concrete id.

        If a single symbol is registered, ``symbol=None`` is allowed
        and resolves to that symbol (legacy backward compatibility).
        Otherwise ``symbol`` must be passed explicitly.
        """

        if symbol is None:
            if len(self._books) == 1:
                return next(iter(self._books))
            raise ValueError(
                "symbol= is required when the engine has multiple "
                "registered symbols (or zero); pass symbol= explicitly"
            )
        return int(symbol)

    def _route_new_order(
        self,
        otype: str,
        order_id: int,
        side: Side,
        price: int,
        qty: int,
        ts: int,
        symbol: Optional[int],
    ) -> list[ExecutionReport]:
        """Route a NewOrder to the correct per-symbol book, or emit a
        ``Reject UNKNOWN_SYMBOL`` if the symbol is not registered.

        After successful dispatch, this method updates the cross-symbol
        id-to-symbol map for any maker that was fully filled (removed
        from a book's ``_orders``) and for the taker if it came to rest
        (still present in the book's ``_orders`` after the call).
        """

        sym = self._resolve_symbol(symbol)
        book = self._books.get(sym)
        if book is None:
            return [
                ExecutionReport(
                    kind=ReportKind.REJECT,
                    order_id=order_id,
                    side=side,
                    price=price,
                    qty=qty,
                    ts=ts,
                    reject_reason=RejectReason.UNKNOWN_SYMBOL,
                )
            ]

        if otype == "limit":
            reports, fully_filled = book.submit_limit(order_id, side, price, qty, ts)
        elif otype == "market":
            reports, fully_filled = book.submit_market(order_id, side, qty, ts)
        elif otype == "ioc":
            reports, fully_filled = book.submit_ioc(order_id, side, price, qty, ts)
        elif otype == "postonly":
            reports, fully_filled = book.submit_post_only(order_id, side, price, qty, ts)
        elif otype == "fok":
            reports, fully_filled = book.submit_fok(order_id, side, price, qty, ts)
        else:
            raise ValueError(f"unknown order type {otype!r}")

        # Cross-symbol id-to-symbol map maintenance.
        for maker_id in fully_filled:
            self._id_to_symbol.pop(maker_id, None)
        # If the taker rested on this book, register it in the
        # cross-symbol map. Only LIMIT can rest under the current scope; checking
        # the book's per-symbol ``_orders`` map is the obvious-over-fast
        # check that matches the actual outcome of the call.
        if order_id in book._orders:
            self._id_to_symbol[order_id] = sym
        return reports


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
