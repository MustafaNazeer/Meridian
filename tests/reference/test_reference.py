"""Worked-example unit tests for the Python matching reference.

Every worked example in ``docs/risk/matching-semantics.md`` becomes one
test method. Each test sets up the initial book state described in the
worked example, submits the event, and asserts that:

1. The list of emitted ``ExecutionReport`` objects matches the
   "Reports emitted (in order)" block of the worked example, byte for
   byte through ``ExecutionReport.to_dict()``.
2. The post-event book state, as observed through ``top_of_book`` and
   the engine's internal ``_orders`` lookup map, matches the
   "Post-event book" block of the worked example.

The bar for the reference is that every worked example passes
(agents/18-reference-implementation-engineer.md, "Definition of done").

Run from the repo root:

    python3 -m unittest discover tests/reference -v
"""

from __future__ import annotations

import os
import sys
import unittest

# Make the matching reference importable when the suite is invoked
# from the repo root via ``python3 -m unittest discover tests/reference``.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from matching_reference import (  # noqa: E402
    ExecutionReport,
    MatchingReference,
    RejectReason,
    ReportKind,
    Side,
)


def _ack(order_id: int, side: Side, price: int, qty: int, ts: int) -> dict:
    return ExecutionReport(
        kind=ReportKind.ACK,
        order_id=order_id,
        side=side,
        price=price,
        qty=qty,
        ts=ts,
    ).to_dict()


def _fill(order_id: int, side: Side, price: int, qty: int, ts: int) -> dict:
    return ExecutionReport(
        kind=ReportKind.FILL,
        order_id=order_id,
        side=side,
        price=price,
        qty=qty,
        ts=ts,
    ).to_dict()


def _cancel(order_id: int, side: Side, price: int, qty: int, ts: int) -> dict:
    return ExecutionReport(
        kind=ReportKind.CANCEL,
        order_id=order_id,
        side=side,
        price=price,
        qty=qty,
        ts=ts,
    ).to_dict()


def _reject(
    order_id: int,
    side: Side,
    price: int,
    qty: int,
    ts: int,
    reason: RejectReason,
) -> dict:
    return ExecutionReport(
        kind=ReportKind.REJECT,
        order_id=order_id,
        side=side,
        price=price,
        qty=qty,
        ts=ts,
        reject_reason=reason,
    ).to_dict()


def _to_dicts(reports) -> list:
    return [r.to_dict() for r in reports]


class _BaseRefTest(unittest.TestCase):
    """Common setup and assertion helpers."""

    def setUp(self) -> None:
        self.engine = MatchingReference(symbol=1)

    # -- helpers for shaping the initial book ---------------------------------

    def _seed_limit(
        self, order_id: int, side: Side, price: int, qty: int, ts: int
    ) -> None:
        """Submit a passive limit and assert it produced exactly one Ack
        and rested. Used to set up the initial book in worked examples
        without coupling the seed asserts to the test under examination.
        """

        reports = self.engine.submit_limit(order_id, side, price, qty, ts)
        self.assertEqual(len(reports), 1, f"seed limit {order_id} should ack only")
        self.assertEqual(reports[0].kind, ReportKind.ACK)


# ---------------------------------------------------------------------------
# Section 3: Limit order semantics (LIM-1 through LIM-7)
# ---------------------------------------------------------------------------


class LimitOrderTests(_BaseRefTest):
    """matching-semantics.md sections 3.1 through 3.7."""

    def test_lim_1_passive_limit_on_empty_book(self) -> None:
        """LIM-1: passive limit on an empty book rests."""

        reports = self.engine.submit_limit(
            order_id=101, side=Side.BUY, price=10000, qty=50, ts=1
        )
        self.assertEqual(
            _to_dicts(reports),
            [_ack(101, Side.BUY, 10000, 50, 1)],
        )
        self.assertEqual(self.engine.top_of_book(), (10000, 50, None, 0))

    def test_lim_2_passive_limit_joining_tail_fifo(self) -> None:
        """LIM-2: a second limit at the same price joins the tail."""

        self._seed_limit(101, Side.BUY, 10000, 50, 1)
        reports = self.engine.submit_limit(
            order_id=102, side=Side.BUY, price=10000, qty=30, ts=2
        )
        self.assertEqual(
            _to_dicts(reports),
            [_ack(102, Side.BUY, 10000, 30, 2)],
        )
        # Both rest at 10000; total qty at the level is 80.
        self.assertEqual(self.engine.top_of_book(), (10000, 80, None, 0))

    def test_lim_3_aggressive_fully_crossing_one_resting(self) -> None:
        """LIM-3: aggressive limit crosses one resting ask in full."""

        self._seed_limit(201, Side.SELL, 10001, 40, 2)
        reports = self.engine.submit_limit(
            order_id=301, side=Side.BUY, price=10001, qty=40, ts=3
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _ack(301, Side.BUY, 10001, 40, 3),
                _fill(201, Side.SELL, 10001, 40, 3),  # maker
                _fill(301, Side.BUY, 10001, 40, 3),  # taker
            ],
        )
        self.assertEqual(self.engine.top_of_book(), (None, 0, None, 0))

    def test_lim_4_sweep_two_levels_with_resting_residual(self) -> None:
        """LIM-4: sweep two ask levels then rest the residual."""

        self._seed_limit(201, Side.SELL, 10001, 30, 1)
        self._seed_limit(202, Side.SELL, 10002, 40, 2)
        self._seed_limit(203, Side.SELL, 10003, 50, 3)

        reports = self.engine.submit_limit(
            order_id=302, side=Side.BUY, price=10002, qty=100, ts=4
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _ack(302, Side.BUY, 10002, 100, 4),
                _fill(201, Side.SELL, 10001, 30, 4),
                _fill(302, Side.BUY, 10001, 30, 4),
                _fill(202, Side.SELL, 10002, 40, 4),
                _fill(302, Side.BUY, 10002, 40, 4),
            ],
        )
        # 30-share residual rests at 10002 on the bid side; 10003 ask
        # untouched with 50.
        self.assertEqual(self.engine.top_of_book(), (10002, 30, 10003, 50))

    def test_lim_5_time_priority_within_a_level(self) -> None:
        """LIM-5: FIFO at a price level (oldest fills first)."""

        self._seed_limit(201, Side.SELL, 10001, 20, 1)
        self._seed_limit(202, Side.SELL, 10001, 20, 2)
        self._seed_limit(203, Side.SELL, 10001, 20, 3)

        reports = self.engine.submit_limit(
            order_id=401, side=Side.BUY, price=10001, qty=35, ts=5
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _ack(401, Side.BUY, 10001, 35, 5),
                _fill(201, Side.SELL, 10001, 20, 5),
                _fill(401, Side.BUY, 10001, 20, 5),
                _fill(202, Side.SELL, 10001, 15, 5),
                _fill(401, Side.BUY, 10001, 15, 5),
            ],
        )
        # 202 has 5 left at the front, 203 still has 20; total at
        # level = 25.
        self.assertEqual(self.engine.top_of_book(), (None, 0, 10001, 25))

    def test_lim_6_sweep_two_orders_at_one_level_then_next_level(self) -> None:
        """LIM-6: aggressor consumes a level fully before going to the
        next level, and partial fills the next level's front."""

        self._seed_limit(201, Side.SELL, 10001, 20, 1)
        self._seed_limit(202, Side.SELL, 10001, 20, 2)
        self._seed_limit(203, Side.SELL, 10002, 50, 3)

        reports = self.engine.submit_limit(
            order_id=402, side=Side.BUY, price=10002, qty=80, ts=6
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _ack(402, Side.BUY, 10002, 80, 6),
                _fill(201, Side.SELL, 10001, 20, 6),
                _fill(402, Side.BUY, 10001, 20, 6),
                _fill(202, Side.SELL, 10001, 20, 6),
                _fill(402, Side.BUY, 10001, 20, 6),
                _fill(203, Side.SELL, 10002, 40, 6),
                _fill(402, Side.BUY, 10002, 40, 6),
            ],
        )
        self.assertEqual(self.engine.top_of_book(), (None, 0, 10002, 10))

    def test_lim_7_sell_aggressor_symmetry(self) -> None:
        """LIM-7: an aggressive sell walks the bid side from the top
        downward and stops at its limit."""

        self._seed_limit(101, Side.BUY, 9999, 60, 1)
        self._seed_limit(102, Side.BUY, 9998, 40, 2)

        reports = self.engine.submit_limit(
            order_id=501, side=Side.SELL, price=9999, qty=60, ts=7
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _ack(501, Side.SELL, 9999, 60, 7),
                _fill(101, Side.BUY, 9999, 60, 7),
                _fill(501, Side.SELL, 9999, 60, 7),
            ],
        )
        # 9998 bid still resting at 40.
        self.assertEqual(self.engine.top_of_book(), (9998, 40, None, 0))


# ---------------------------------------------------------------------------
# Section 4: Market order semantics (MKT-1 through MKT-6)
# ---------------------------------------------------------------------------


class MarketOrderTests(_BaseRefTest):
    """matching-semantics.md sections 4.2 through 4.7."""

    def test_mkt_1_market_buy_full_fill_against_one_resting_ask(self) -> None:
        """MKT-1: market buy fills against a single resting ask in full."""

        self._seed_limit(201, Side.SELL, 10001, 50, 1)
        reports = self.engine.submit_market(
            order_id=601, side=Side.BUY, qty=50, ts=8
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _ack(601, Side.BUY, 0, 50, 8),
                _fill(201, Side.SELL, 10001, 50, 8),
                _fill(601, Side.BUY, 10001, 50, 8),
            ],
        )
        self.assertEqual(self.engine.top_of_book(), (None, 0, None, 0))

    def test_mkt_2_market_buy_sweeping_multiple_levels(self) -> None:
        """MKT-2: market buy sweeps levels with no price limit, walking
        past gaps."""

        self._seed_limit(201, Side.SELL, 10001, 20, 1)
        self._seed_limit(202, Side.SELL, 10002, 30, 2)
        self._seed_limit(203, Side.SELL, 10005, 40, 3)

        reports = self.engine.submit_market(
            order_id=602, side=Side.BUY, qty=80, ts=9
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _ack(602, Side.BUY, 0, 80, 9),
                _fill(201, Side.SELL, 10001, 20, 9),
                _fill(602, Side.BUY, 10001, 20, 9),
                _fill(202, Side.SELL, 10002, 30, 9),
                _fill(602, Side.BUY, 10002, 30, 9),
                _fill(203, Side.SELL, 10005, 30, 9),
                _fill(602, Side.BUY, 10005, 30, 9),
            ],
        )
        self.assertEqual(self.engine.top_of_book(), (None, 0, 10005, 10))

    def test_mkt_3_market_buy_partial_fill_residual_implicit_cancel(self) -> None:
        """MKT-3: market buy partially fills, residual implicitly
        cancelled (no further report)."""

        self._seed_limit(201, Side.SELL, 10001, 30, 1)
        reports = self.engine.submit_market(
            order_id=603, side=Side.BUY, qty=100, ts=10
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _ack(603, Side.BUY, 0, 100, 10),
                _fill(201, Side.SELL, 10001, 30, 10),
                _fill(603, Side.BUY, 10001, 30, 10),
            ],
        )
        self.assertEqual(self.engine.top_of_book(), (None, 0, None, 0))

    def test_mkt_4_market_buy_against_empty_book_is_rejected(self) -> None:
        """MKT-4: market buy against an empty book is a single Reject
        with reason EmptyBook, no preceding Acknowledge."""

        reports = self.engine.submit_market(
            order_id=604, side=Side.BUY, qty=50, ts=11
        )
        self.assertEqual(
            _to_dicts(reports),
            [_reject(604, Side.BUY, 0, 50, 11, RejectReason.EMPTY_BOOK)],
        )
        self.assertEqual(self.engine.top_of_book(), (None, 0, None, 0))

    def test_mkt_5_market_sell_full_fill_against_one_resting_bid(self) -> None:
        """MKT-5: market sell fills against a single resting bid in full."""

        self._seed_limit(101, Side.BUY, 9999, 40, 1)
        reports = self.engine.submit_market(
            order_id=605, side=Side.SELL, qty=40, ts=12
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _ack(605, Side.SELL, 0, 40, 12),
                _fill(101, Side.BUY, 9999, 40, 12),
                _fill(605, Side.SELL, 9999, 40, 12),
            ],
        )
        self.assertEqual(self.engine.top_of_book(), (None, 0, None, 0))

    def test_mkt_6_market_sell_against_empty_bid_side_is_rejected(self) -> None:
        """MKT-6: a market sell with no bids is rejected even if there
        is volume on the ask side. Only the opposite side matters."""

        self._seed_limit(201, Side.SELL, 10001, 50, 1)
        reports = self.engine.submit_market(
            order_id=606, side=Side.SELL, qty=50, ts=13
        )
        self.assertEqual(
            _to_dicts(reports),
            [_reject(606, Side.SELL, 0, 50, 13, RejectReason.EMPTY_BOOK)],
        )
        # Ask side unchanged.
        self.assertEqual(self.engine.top_of_book(), (None, 0, 10001, 50))


# ---------------------------------------------------------------------------
# Section 5: IOC order semantics (IOC-1 through IOC-7)
# ---------------------------------------------------------------------------


class IocOrderTests(_BaseRefTest):
    """matching-semantics.md sections 5.1 through 5.7."""

    def test_ioc_1_full_fill_against_one_resting_ask(self) -> None:
        """IOC-1: IOC fully fills against a single resting order."""

        self._seed_limit(201, Side.SELL, 10001, 30, 1)
        reports = self.engine.submit_ioc(
            order_id=701, side=Side.BUY, price=10001, qty=30, ts=14
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _ack(701, Side.BUY, 10001, 30, 14),
                _fill(201, Side.SELL, 10001, 30, 14),
                _fill(701, Side.BUY, 10001, 30, 14),
            ],
        )
        self.assertEqual(self.engine.top_of_book(), (None, 0, None, 0))

    def test_ioc_2_partial_fill_residual_implicit_cancel(self) -> None:
        """IOC-2: IOC partial fill, residual implicitly cancelled."""

        self._seed_limit(201, Side.SELL, 10001, 20, 1)
        reports = self.engine.submit_ioc(
            order_id=702, side=Side.BUY, price=10001, qty=50, ts=15
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _ack(702, Side.BUY, 10001, 50, 15),
                _fill(201, Side.SELL, 10001, 20, 15),
                _fill(702, Side.BUY, 10001, 20, 15),
            ],
        )
        self.assertEqual(self.engine.top_of_book(), (None, 0, None, 0))

    def test_ioc_3_against_empty_book_acks_then_implicit_cancel(self) -> None:
        """IOC-3: IOC against an empty book is acknowledged (with
        submitted qty) and then implicitly cancelled in full. Critical
        corner case: differs from market (which is rejected)."""

        reports = self.engine.submit_ioc(
            order_id=703, side=Side.BUY, price=10001, qty=50, ts=16
        )
        self.assertEqual(
            _to_dicts(reports),
            [_ack(703, Side.BUY, 10001, 50, 16)],
        )
        self.assertEqual(self.engine.top_of_book(), (None, 0, None, 0))

    def test_ioc_4_at_price_that_does_not_cross(self) -> None:
        """IOC-4: IOC at a price that does not cross any resting level
        is acknowledged then implicitly cancelled in full; book is
        unchanged."""

        self._seed_limit(201, Side.SELL, 10005, 30, 1)
        reports = self.engine.submit_ioc(
            order_id=704, side=Side.BUY, price=10001, qty=30, ts=17
        )
        self.assertEqual(
            _to_dicts(reports),
            [_ack(704, Side.BUY, 10001, 30, 17)],
        )
        self.assertEqual(self.engine.top_of_book(), (None, 0, 10005, 30))

    def test_ioc_5_sweep_two_levels_finishing_at_limit(self) -> None:
        """IOC-5: IOC sweeps two levels then implicitly cancels the
        residual at the limit boundary."""

        self._seed_limit(201, Side.SELL, 10001, 20, 1)
        self._seed_limit(202, Side.SELL, 10002, 30, 2)
        self._seed_limit(203, Side.SELL, 10003, 40, 3)

        reports = self.engine.submit_ioc(
            order_id=705, side=Side.BUY, price=10002, qty=60, ts=18
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _ack(705, Side.BUY, 10002, 60, 18),
                _fill(201, Side.SELL, 10001, 20, 18),
                _fill(705, Side.BUY, 10001, 20, 18),
                _fill(202, Side.SELL, 10002, 30, 18),
                _fill(705, Side.BUY, 10002, 30, 18),
            ],
        )
        # 10003 ask untouched at 40.
        self.assertEqual(self.engine.top_of_book(), (None, 0, 10003, 40))

    def test_ioc_6_full_fill_within_a_single_level_fifo(self) -> None:
        """IOC-6: IOC fully fills within a single level honoring FIFO."""

        self._seed_limit(201, Side.SELL, 10001, 20, 1)
        self._seed_limit(202, Side.SELL, 10001, 20, 2)
        self._seed_limit(203, Side.SELL, 10001, 20, 3)

        reports = self.engine.submit_ioc(
            order_id=706, side=Side.BUY, price=10001, qty=40, ts=19
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _ack(706, Side.BUY, 10001, 40, 19),
                _fill(201, Side.SELL, 10001, 20, 19),
                _fill(706, Side.BUY, 10001, 20, 19),
                _fill(202, Side.SELL, 10001, 20, 19),
                _fill(706, Side.BUY, 10001, 20, 19),
            ],
        )
        # 203 still resting at front of 10001 with 20.
        self.assertEqual(self.engine.top_of_book(), (None, 0, 10001, 20))

    def test_ioc_7_sell_partial_fill_against_multiple_bid_levels(self) -> None:
        """IOC-7: IOC sell partially fills against bids and stops at the
        limit boundary (sell at 9998 will not trade at 9995)."""

        self._seed_limit(101, Side.BUY, 9999, 30, 1)
        self._seed_limit(102, Side.BUY, 9998, 20, 2)
        self._seed_limit(103, Side.BUY, 9995, 50, 3)

        reports = self.engine.submit_ioc(
            order_id=707, side=Side.SELL, price=9998, qty=80, ts=20
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _ack(707, Side.SELL, 9998, 80, 20),
                _fill(101, Side.BUY, 9999, 30, 20),
                _fill(707, Side.SELL, 9999, 30, 20),
                _fill(102, Side.BUY, 9998, 20, 20),
                _fill(707, Side.SELL, 9998, 20, 20),
            ],
        )
        # 9995 bid untouched.
        self.assertEqual(self.engine.top_of_book(), (9995, 50, None, 0))


# ---------------------------------------------------------------------------
# Section 6: Cancel-by-id semantics (CXL-1 through CXL-6)
# ---------------------------------------------------------------------------


class CancelTests(_BaseRefTest):
    """matching-semantics.md sections 6.1 through 6.6."""

    def test_cxl_1_cancel_fully_resting_order(self) -> None:
        """CXL-1: cancel removes a fully resting order; next-in-queue
        advances to the front of the level."""

        self._seed_limit(101, Side.BUY, 10000, 50, 1)
        self._seed_limit(102, Side.BUY, 10000, 30, 2)

        reports = self.engine.cancel_at(101, ts=21)
        self.assertEqual(
            _to_dicts(reports),
            [_cancel(101, Side.BUY, 10000, 50, 21)],
        )
        # 102 remains at 10000 with 30.
        self.assertEqual(self.engine.top_of_book(), (10000, 30, None, 0))

    def test_cxl_2_cancel_partially_filled_resting_order(self) -> None:
        """CXL-2: a partially filled resting order's Cancel report
        carries the remaining qty (20), not the original submitted
        qty (50)."""

        # Set up: order 101 buy limit at 10000 for 50.
        self._seed_limit(101, Side.BUY, 10000, 50, 1)
        # Add 102 behind 101 at the same level.
        self._seed_limit(102, Side.BUY, 10000, 30, 2)
        # Aggressor sell consumes 30 of 101.
        sell_reports = self.engine.submit_limit(
            order_id=901, side=Side.SELL, price=10000, qty=30, ts=10
        )
        # Sanity: the sell fully consumes 30 of 101 and rests nothing.
        self.assertEqual(len(sell_reports), 3)  # ack + 2 fills
        # Now cancel 101.
        reports = self.engine.cancel_at(101, ts=22)
        self.assertEqual(
            _to_dicts(reports),
            [_cancel(101, Side.BUY, 10000, 20, 22)],
        )
        self.assertEqual(self.engine.top_of_book(), (10000, 30, None, 0))

    def test_cxl_3_cancel_removes_last_order_at_a_level(self) -> None:
        """CXL-3: cancelling the last order at a level removes the
        level itself."""

        self._seed_limit(101, Side.BUY, 10000, 50, 1)
        reports = self.engine.cancel_at(101, ts=23)
        self.assertEqual(
            _to_dicts(reports),
            [_cancel(101, Side.BUY, 10000, 50, 23)],
        )
        self.assertEqual(self.engine.top_of_book(), (None, 0, None, 0))

    def test_cxl_4_cancel_unknown_id_reject_not_found(self) -> None:
        """CXL-4: cancel of an unknown id is a Reject NotFound with
        sentinel side BUY and price 0 (per matching-semantics.md
        section 8.2 convention)."""

        self._seed_limit(101, Side.BUY, 10000, 50, 1)
        reports = self.engine.cancel_at(999, ts=24)
        self.assertEqual(
            _to_dicts(reports),
            [_reject(999, Side.BUY, 0, 0, 24, RejectReason.NOT_FOUND)],
        )
        # Book unchanged.
        self.assertEqual(self.engine.top_of_book(), (10000, 50, None, 0))

    def test_cxl_5_cancel_already_fully_filled_id(self) -> None:
        """CXL-5: cancel of an id that already fully filled is a
        Reject NotFound (the engine cannot tell filled apart from
        never-existed)."""

        # Set up: 201 sells 30 at 10001, 301 buys 30 at 10001 (full
        # cross). 201 is no longer in the lookup map.
        self._seed_limit(201, Side.SELL, 10001, 30, 1)
        cross = self.engine.submit_limit(
            order_id=301, side=Side.BUY, price=10001, qty=30, ts=2
        )
        self.assertEqual(len(cross), 3)
        # Now cancel 201.
        reports = self.engine.cancel_at(201, ts=25)
        self.assertEqual(
            _to_dicts(reports),
            [_reject(201, Side.BUY, 0, 0, 25, RejectReason.NOT_FOUND)],
        )
        self.assertEqual(self.engine.top_of_book(), (None, 0, None, 0))

    def test_cxl_6_cancel_already_cancelled_id(self) -> None:
        """CXL-6: a second cancel of the same id returns Reject
        NotFound (cancel idempotence)."""

        self._seed_limit(101, Side.BUY, 10000, 50, 1)
        self._seed_limit(102, Side.BUY, 10000, 30, 2)
        first = self.engine.cancel_at(101, ts=21)
        self.assertEqual(len(first), 1)
        self.assertEqual(first[0].kind, ReportKind.CANCEL)
        # Second cancel of 101 is a Reject NotFound.
        reports = self.engine.cancel_at(101, ts=26)
        self.assertEqual(
            _to_dicts(reports),
            [_reject(101, Side.BUY, 0, 0, 26, RejectReason.NOT_FOUND)],
        )
        # 102 is still resting.
        self.assertEqual(self.engine.top_of_book(), (10000, 30, None, 0))


# ---------------------------------------------------------------------------
# Section 7.1: conservation law on a few worked examples
# ---------------------------------------------------------------------------


class ConservationLawTests(_BaseRefTest):
    """Cross-check matching-semantics.md section 7.1 worked walkthroughs.

    The conservation law ``submitted = filled + resting + cancelled``
    is checked here on a few examples by reading the engine state
    after the event. This does not replace the QA Engineer's Phase 3
    property tests; it is a sanity check at unit-test scale.
    """

    def test_mkt_3_conservation(self) -> None:
        """MKT-3: 100 = 30 filled + 0 resting + 70 cancelled."""

        self._seed_limit(201, Side.SELL, 10001, 30, 1)
        reports = self.engine.submit_market(
            order_id=603, side=Side.BUY, qty=100, ts=10
        )
        # Filled qty: sum of taker-side fills for 603.
        filled = sum(
            r.qty
            for r in reports
            if r.kind == ReportKind.FILL and r.order_id == 603
        )
        self.assertEqual(filled, 30)
        # Order 603 is a market order; it never rests. Confirm via
        # the lookup map. (Internal API; reasonable for a same-package
        # invariant test.)
        self.assertNotIn(603, self.engine._orders)
        cancelled = 100 - filled  # implicit cancel of residual
        self.assertEqual(100, filled + 0 + cancelled)

    def test_ioc_7_conservation(self) -> None:
        """IOC-7: 80 = 50 filled + 0 resting + 30 cancelled."""

        self._seed_limit(101, Side.BUY, 9999, 30, 1)
        self._seed_limit(102, Side.BUY, 9998, 20, 2)
        self._seed_limit(103, Side.BUY, 9995, 50, 3)
        reports = self.engine.submit_ioc(
            order_id=707, side=Side.SELL, price=9998, qty=80, ts=20
        )
        filled = sum(
            r.qty
            for r in reports
            if r.kind == ReportKind.FILL and r.order_id == 707
        )
        self.assertEqual(filled, 50)
        self.assertNotIn(707, self.engine._orders)
        cancelled = 80 - filled
        self.assertEqual(80, filled + 0 + cancelled)


# ---------------------------------------------------------------------------
# Phase 2: multi-symbol dispatch and cross-symbol cancel
# ---------------------------------------------------------------------------


class MultiSymbolTests(unittest.TestCase):
    """Phase 2 plan task 5 cases: 5-symbol dispatch, cross-symbol cancel
    isolation, cross-symbol cancel by id, and unknown-symbol reject.

    The chosen design is option (a) from the Phase 2 dispatch prompt:
    ``MatchingReference`` accepts a list of symbols at construction.
    The single-symbol form ``MatchingReference(symbol=1)`` continues to
    work; every Phase 1 worked-example test exercises it. The
    multi-symbol form ``MatchingReference(symbols=[1, 2, 3, 4, 5])``
    registers a fixed set of per-symbol books and routes ``submit_*``
    by an explicit ``symbol=`` keyword. Cancel is cross-symbol: it
    consults a top-level id-to-symbol map and dispatches to the right
    book without the caller naming the symbol.
    """

    DEMO_SYMBOLS = (1, 2, 3, 4, 5)

    def setUp(self) -> None:
        self.engine = MatchingReference(symbols=self.DEMO_SYMBOLS)

    def test_five_symbol_dispatch_no_leakage(self) -> None:
        """One limit order per symbol; verify each rests on its own
        book without leaking into another symbol's book."""

        # Submit one buy limit on each demo symbol, all at distinct
        # prices and quantities so a leak would be observable.
        plan = [
            (101, 1, 10000, 50),
            (201, 2, 20000, 30),
            (301, 3, 30000, 70),
            (401, 4, 40000, 20),
            (501, 5, 50000, 90),
        ]
        for ts, (oid, sym, price, qty) in enumerate(plan, start=1):
            reports = self.engine.submit_limit(
                oid, Side.BUY, price, qty, ts, symbol=sym
            )
            self.assertEqual(
                _to_dicts(reports),
                [_ack(oid, Side.BUY, price, qty, ts)],
                f"symbol {sym} should ack and rest only",
            )

        # Each symbol's book holds exactly its own resting buy and
        # nothing else.
        for oid, sym, price, qty in plan:
            self.assertEqual(
                self.engine.top_of_book(symbol=sym),
                (price, qty, None, 0),
                f"symbol {sym}: bid {price}x{qty}, ask empty",
            )

        # The cross-symbol id-to-symbol map carries every resting id.
        self.assertEqual(
            self.engine._id_to_symbol,
            {oid: sym for oid, sym, _, _ in plan},
        )

    def test_cross_symbol_cancel_isolation(self) -> None:
        """Cancel on symbol 1 must not affect symbol 2's book."""

        # Place a buy on symbol 1 and a buy on symbol 2.
        self.engine.submit_limit(101, Side.BUY, 10000, 50, 1, symbol=1)
        self.engine.submit_limit(202, Side.BUY, 20000, 30, 2, symbol=2)

        # Cancel the symbol-1 buy via the cross-symbol cancel API
        # (the caller does not name symbol 1).
        reports = self.engine.cancel_at(101, ts=21)
        self.assertEqual(
            _to_dicts(reports),
            [_cancel(101, Side.BUY, 10000, 50, 21)],
        )

        # Symbol 1's book is now empty.
        self.assertEqual(
            self.engine.top_of_book(symbol=1), (None, 0, None, 0)
        )
        # Symbol 2's book is unchanged.
        self.assertEqual(
            self.engine.top_of_book(symbol=2), (20000, 30, None, 0)
        )
        # Cross-symbol map: 101 gone, 202 still routes to symbol 2.
        self.assertEqual(self.engine._id_to_symbol, {202: 2})

    def test_cross_symbol_cancel_by_id_routes_correctly(self) -> None:
        """Cancel an order on symbol 5 without naming the symbol;
        the engine routes correctly via its id-to-symbol map."""

        # Seed orders on symbols 1, 3, and 5 so a wrong route would
        # be observable on either neighbor.
        self.engine.submit_limit(11, Side.BUY, 10000, 50, 1, symbol=1)
        self.engine.submit_limit(33, Side.BUY, 30000, 40, 2, symbol=3)
        self.engine.submit_limit(55, Side.SELL, 50500, 25, 3, symbol=5)

        # Cancel the symbol-5 sell with no symbol argument.
        reports = self.engine.cancel_at(55, ts=42)
        self.assertEqual(
            _to_dicts(reports),
            [_cancel(55, Side.SELL, 50500, 25, 42)],
        )

        # Symbols 1 and 3 untouched.
        self.assertEqual(
            self.engine.top_of_book(symbol=1), (10000, 50, None, 0)
        )
        self.assertEqual(
            self.engine.top_of_book(symbol=3), (30000, 40, None, 0)
        )
        # Symbol 5's ask is now empty.
        self.assertEqual(
            self.engine.top_of_book(symbol=5), (None, 0, None, 0)
        )

    def test_unknown_symbol_reject_no_preceding_ack(self) -> None:
        """NewOrder with symbol=99 (unregistered) emits a single
        ``Reject UNKNOWN_SYMBOL`` with no preceding ``Acknowledge``,
        matching the EmptyBook reject pattern from MKT-4."""

        reports = self.engine.submit_limit(
            999, Side.BUY, 12345, 10, 7, symbol=99
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _reject(
                    999, Side.BUY, 12345, 10, 7, RejectReason.UNKNOWN_SYMBOL
                )
            ],
        )
        # No book state was created or mutated for symbol 99; the
        # registered books are untouched.
        for sym in self.DEMO_SYMBOLS:
            self.assertEqual(
                self.engine.top_of_book(symbol=sym), (None, 0, None, 0)
            )
        # And the unknown-symbol order id is not in the cross-symbol
        # map (the reject happens before any state mutation).
        self.assertNotIn(999, self.engine._id_to_symbol)

    def test_unknown_symbol_reject_market_uses_zero_price(self) -> None:
        """Market NewOrder with an unknown symbol emits ``Reject
        UNKNOWN_SYMBOL`` with ``price=0`` (the same convention market
        orders use for ``Acknowledge``)."""

        reports = self.engine.submit_market(
            888, Side.SELL, 50, 9, symbol=42
        )
        self.assertEqual(
            _to_dicts(reports),
            [
                _reject(
                    888, Side.SELL, 0, 50, 9, RejectReason.UNKNOWN_SYMBOL
                )
            ],
        )

    def test_cross_symbol_cancel_unknown_id_rejects_not_found(self) -> None:
        """Cancel of an id that was never submitted on any symbol is a
        ``Reject NOT_FOUND``. CXL-4 generalized to multi-symbol mode."""

        self.engine.submit_limit(101, Side.BUY, 10000, 50, 1, symbol=1)
        reports = self.engine.cancel_at(424242, ts=11)
        self.assertEqual(
            _to_dicts(reports),
            [_reject(424242, Side.BUY, 0, 0, 11, RejectReason.NOT_FOUND)],
        )
        # Symbol 1's book unchanged.
        self.assertEqual(
            self.engine.top_of_book(symbol=1), (10000, 50, None, 0)
        )

    def test_same_id_can_be_reused_on_different_symbol_after_cancel(
        self,
    ) -> None:
        """After 101 is cancelled on symbol 1, the id is free again
        and can be assigned to a fresh order on symbol 2.

        This is a Phase 2 invariant the C++ side gets via the
        ``OrderIndex::erase`` path; the reference must agree.
        """

        self.engine.submit_limit(101, Side.BUY, 10000, 50, 1, symbol=1)
        self.engine.cancel_at(101, ts=2)
        # Re-use 101 on symbol 2.
        self.engine.submit_limit(101, Side.BUY, 20000, 30, 3, symbol=2)
        # Now cancel via the cross-symbol API; routes to symbol 2.
        reports = self.engine.cancel_at(101, ts=4)
        self.assertEqual(
            _to_dicts(reports),
            [_cancel(101, Side.BUY, 20000, 30, 4)],
        )

    def test_per_symbol_books_match_independently(self) -> None:
        """Run a small mixed corpus on symbol 1 and symbol 2 in
        interleaved order; the per-symbol final book states match what
        each symbol would have produced in isolation.

        This pins the "no cross-symbol leakage" invariant from the
        Phase 2 plan Task 6 scenario list (item 1: independent
        activity per symbol)."""

        # Symbol 1: a buy at 10000 partial-filled by a 30 sell.
        self.engine.submit_limit(11, Side.BUY, 10000, 50, 1, symbol=1)
        self.engine.submit_limit(12, Side.SELL, 10000, 30, 2, symbol=1)
        # Interleave a symbol-2 sell.
        self.engine.submit_limit(21, Side.SELL, 20000, 40, 3, symbol=2)
        # Continue symbol 1 with a fresh buy that rests behind 11.
        self.engine.submit_limit(13, Side.BUY, 9999, 25, 4, symbol=1)
        # Continue symbol 2 with a buy that crosses 21 in full.
        self.engine.submit_limit(22, Side.BUY, 20000, 40, 5, symbol=2)

        # Symbol 1 final state: 11 has 20 remaining at 10000 (front),
        # 13 has 25 at 9999. Asks empty.
        self.assertEqual(
            self.engine.top_of_book(symbol=1), (10000, 20, None, 0)
        )
        # Symbol 2 final state: both sides empty (21 and 22 fully
        # crossed at 20000).
        self.assertEqual(
            self.engine.top_of_book(symbol=2), (None, 0, None, 0)
        )

    def test_legacy_single_symbol_constructor_still_works(self) -> None:
        """Backward compatibility: ``MatchingReference(symbol=1)``
        keeps the Phase 1 single-symbol API. ``submit_*`` and
        ``cancel_at`` work without a ``symbol=`` keyword."""

        legacy = MatchingReference(symbol=1)
        legacy.submit_limit(101, Side.BUY, 10000, 50, 1)
        reports = legacy.cancel_at(101, ts=2)
        self.assertEqual(
            _to_dicts(reports),
            [_cancel(101, Side.BUY, 10000, 50, 2)],
        )
        self.assertEqual(legacy.top_of_book(), (None, 0, None, 0))

    def test_constructor_rejects_both_symbol_and_symbols(self) -> None:
        """Programmer-error guard: passing both forms is a ValueError."""

        with self.assertRaises(ValueError):
            MatchingReference(symbol=1, symbols=[1, 2])

    def test_multi_symbol_requires_explicit_symbol_on_submit(self) -> None:
        """In multi-symbol mode, omitting ``symbol=`` on a
        ``submit_*`` is a programmer error (ValueError)."""

        with self.assertRaises(ValueError):
            self.engine.submit_limit(1, Side.BUY, 10000, 10, 1)


# ---------------------------------------------------------------------------
# Determinism: byte-identical JSON across runs
# ---------------------------------------------------------------------------


class DeterminismTests(unittest.TestCase):
    """The integration test diffs JSON byte-for-byte. Confirm here that
    the same input always produces the same JSON output across two
    independent engine instances."""

    def _run_corpus(self) -> str:
        import json

        engine = MatchingReference(symbol=1)
        events = []
        # A small mixed corpus exercising every Phase 1 path.
        events.extend(engine.submit_limit(101, Side.BUY, 10000, 50, 1))
        events.extend(engine.submit_limit(102, Side.BUY, 10000, 30, 2))
        events.extend(engine.submit_limit(201, Side.SELL, 10001, 40, 3))
        events.extend(engine.submit_limit(301, Side.BUY, 10001, 40, 4))
        events.extend(engine.submit_market(401, Side.SELL, 30, 5))
        events.extend(engine.submit_ioc(501, Side.BUY, 10001, 50, 6))
        events.extend(engine.cancel_at(102, ts=7))
        events.extend(engine.cancel_at(999, ts=8))
        return "\n".join(
            json.dumps(r.to_dict(), sort_keys=True) for r in events
        )

    def test_byte_identical_across_runs(self) -> None:
        a = self._run_corpus()
        b = self._run_corpus()
        self.assertEqual(a, b)


if __name__ == "__main__":
    unittest.main()
