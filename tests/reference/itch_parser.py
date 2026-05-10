"""Python ITCH 5.0 parser. Mirror of include/meridian/itch.hpp.

Same scope as the C++ side: decodes Add Order ('A') and Order Delete
('D') messages, recognizes System Event ('S') and skips it, skips any
other type by length. Unknown types do not cause a parse failure as
long as the length prefix is present and the buffer carries the
declared bytes.

The byte layout follows the NASDAQ ITCH 5.0 spec; see the header for
the field-by-field reference. This file uses ``struct`` (>= big endian)
for decoding, which on a 64-bit platform compiles to the same byte
swap operations the C++ side does by hand.
"""

from __future__ import annotations

import dataclasses
import struct
from dataclasses import dataclass
from enum import Enum
from typing import Iterator, Optional


class MessageKind(Enum):
    ADD_ORDER = "add_order"
    ORDER_DELETE = "order_delete"
    OTHER_SKIPPED = "other_skipped"


class ParseStatus(Enum):
    OK = "ok"
    END_OF_STREAM = "end_of_stream"
    TRUNCATED_FRAME = "truncated_frame"
    INVALID_LENGTH = "invalid_length"
    LENGTH_MISMATCH = "length_mismatch"


# Sizes of the message bodies (including the 1-byte type field).
ADD_ORDER_SIZE = 36
ORDER_DELETE_SIZE = 19
SYSTEM_EVENT_SIZE = 12


@dataclass
class AddOrderMsg:
    ts_ns: int
    order_ref: int
    side: str       # "buy" or "sell"
    shares: int
    stock: bytes    # 8-byte left-justified, space-padded
    price: int      # raw uint32 ITCH price (1/10000 dollar units)


@dataclass
class OrderDeleteMsg:
    ts_ns: int
    order_ref: int


@dataclass
class ParsedMessage:
    kind: MessageKind
    add: Optional[AddOrderMsg] = None
    delete: Optional[OrderDeleteMsg] = None


# Big-endian 6-byte unsigned reader. ITCH timestamps are nanoseconds-
# since-midnight packed into 6 bytes; struct does not have a native
# 6-byte type, so we left-pad with two zero bytes and decode as ">Q".
def _be48(b: bytes) -> int:
    return int.from_bytes(b, "big", signed=False)


def parse_stream(buf: bytes) -> Iterator[tuple[ParseStatus, Optional[ParsedMessage], int]]:
    """Yield ``(status, message_or_none, cursor_after_yield)`` tuples.

    The cursor is byte offset of the *next* unread byte after the
    yielded result. On any non-OK status, the cursor is left at the
    start of the offending frame so a caller can diagnose the offset.
    Iteration stops after yielding END_OF_STREAM.
    """

    n = len(buf)
    i = 0
    while True:
        if i >= n:
            yield ParseStatus.END_OF_STREAM, None, i
            return
        if n - i < 2:
            yield ParseStatus.TRUNCATED_FRAME, None, i
            return
        length = struct.unpack_from(">H", buf, i)[0]
        if length < 1:
            yield ParseStatus.INVALID_LENGTH, None, i
            return
        if n - i < 2 + length:
            yield ParseStatus.TRUNCATED_FRAME, None, i
            return
        body_start = i + 2
        msg_type = buf[body_start:body_start + 1]

        if msg_type == b"A":
            if length != ADD_ORDER_SIZE:
                yield ParseStatus.LENGTH_MISMATCH, None, i
                return
            ts_ns = _be48(buf[body_start + 5:body_start + 11])
            order_ref = struct.unpack_from(">Q", buf, body_start + 11)[0]
            bs = buf[body_start + 19:body_start + 20]
            if bs not in (b"B", b"S"):
                yield ParseStatus.LENGTH_MISMATCH, None, i
                return
            side = "buy" if bs == b"B" else "sell"
            shares = struct.unpack_from(">I", buf, body_start + 20)[0]
            stock = bytes(buf[body_start + 24:body_start + 32])
            price = struct.unpack_from(">I", buf, body_start + 32)[0]
            yield (
                ParseStatus.OK,
                ParsedMessage(
                    kind=MessageKind.ADD_ORDER,
                    add=AddOrderMsg(
                        ts_ns=ts_ns, order_ref=order_ref, side=side,
                        shares=shares, stock=stock, price=price,
                    ),
                ),
                i + 2 + length,
            )
        elif msg_type == b"D":
            if length != ORDER_DELETE_SIZE:
                yield ParseStatus.LENGTH_MISMATCH, None, i
                return
            ts_ns = _be48(buf[body_start + 5:body_start + 11])
            order_ref = struct.unpack_from(">Q", buf, body_start + 11)[0]
            yield (
                ParseStatus.OK,
                ParsedMessage(
                    kind=MessageKind.ORDER_DELETE,
                    delete=OrderDeleteMsg(ts_ns=ts_ns, order_ref=order_ref),
                ),
                i + 2 + length,
            )
        elif msg_type == b"S":
            if length != SYSTEM_EVENT_SIZE:
                yield ParseStatus.LENGTH_MISMATCH, None, i
                return
            yield (
                ParseStatus.OK,
                ParsedMessage(kind=MessageKind.OTHER_SKIPPED),
                i + 2 + length,
            )
        else:
            # Unknown type. Skip over the body using the length prefix.
            yield (
                ParseStatus.OK,
                ParsedMessage(kind=MessageKind.OTHER_SKIPPED),
                i + 2 + length,
            )
        i += 2 + length


__all__ = [
    "MessageKind",
    "ParseStatus",
    "AddOrderMsg",
    "OrderDeleteMsg",
    "ParsedMessage",
    "parse_stream",
    "ADD_ORDER_SIZE",
    "ORDER_DELETE_SIZE",
    "SYSTEM_EVENT_SIZE",
]
