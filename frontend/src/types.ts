// Wire-level message shapes broadcast by `meridian-server` over /ws.
//
// The server emits two message kinds today: `snapshot` (the most recent
// top of book picture; replayed to every newly connected client) and
// `delta` (the same shape, broadcast at 30 Hz). Both kinds carry only
// best bid / best ask price and quantity plus an engine timestamp.
// `bid_px === -1` (or `ask_px === -1`) is the wire encoding for an
// empty side; readers must treat that as "no top of book on this side"
// rather than as a real price.

export type TopOfBookMessage = {
  kind: 'snapshot' | 'delta';
  bid_px: number;
  bid_qty: number;
  ask_px: number;
  ask_qty: number;
  ts: number;
};

export type ServerMessage = TopOfBookMessage;

// Four-state connection state machine. The header dot, the hero, and
// the connection banner read this state to render their per-state
// surfaces. See docs/design/wireframes.md "Connection state surface
// integration".
//
// `connecting` is the initial state and the state entered on a fresh
// socket; it flips to `live` on the first `snapshot` message. `live`
// flips to `stalled` if no message arrives in 1 second; the next
// message returns it to `live`. Any socket close or error path lands
// in `disconnected`, which schedules an exponential backoff reconnect.
export type ConnectionState =
  | 'connecting'
  | 'live'
  | 'stalled'
  | 'disconnected';

export type Symbol = 'AAPL' | 'SPY' | 'NVDA' | 'TSLA' | 'GOOG';

export const SYMBOLS: readonly Symbol[] = [
  'AAPL',
  'SPY',
  'NVDA',
  'TSLA',
  'GOOG',
];

// A `snapshot`/`delta` pair from the wire after decoding the empty-side
// sentinel. Both sides are nullable in the post-decode form so the rest
// of the app does not have to repeatedly check for `-1`.
export type TopOfBook = {
  bidPx: number | null;
  bidQty: number;
  askPx: number | null;
  askQty: number;
  ts: number;
};

export function decodeTopOfBook(msg: TopOfBookMessage): TopOfBook {
  return {
    bidPx: msg.bid_px === -1 ? null : msg.bid_px,
    bidQty: msg.bid_qty,
    askPx: msg.ask_px === -1 ? null : msg.ask_px,
    askQty: msg.ask_qty,
    ts: msg.ts,
  };
}
