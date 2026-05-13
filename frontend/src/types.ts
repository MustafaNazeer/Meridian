// Wire-level message shapes broadcast by `meridian-server` over /ws.
//
// v1.1: the envelope grew from a flat TOB-only shape to a nested
// envelope with four sections (tob, depth, trades, latency). Both
// `snapshot` and `delta` kinds share this shape; `snapshot` is just
// the first frame a newly connected client receives. The wire
// encodings keep the v1 conventions where they applied: `bid_px === -1`
// (or `ask_px === -1`) signals an empty side; trade aggressor is
// `"B"` or `"S"` matching the C++ Side enum.

export type WireTob = {
  bid_px: number;
  bid_qty: number;
  ask_px: number;
  ask_qty: number;
};

export type WireDepth = {
  bids: [number, number][];
  asks: [number, number][];
};

export type WireTrade = {
  ts: number;
  px: number;
  qty: number;
  aggressor: 'B' | 'S';
};

export type WireLatency = {
  p50_ns: number;
  p99_ns: number;
  p999_ns: number;
  max_ns: number;
  samples: number;
  hist: number[];
};

export type ServerMessage = {
  kind: 'snapshot' | 'delta';
  ts: number;
  tob: WireTob;
  depth: WireDepth;
  trades: WireTrade[];
  latency: WireLatency;
};

// Post-decode shapes. The frontend works in these forms after
// sentinel handling and field renaming.
export type TopOfBook = {
  bidPx: number | null;
  bidQty: number;
  askPx: number | null;
  askQty: number;
  ts: number;
};

export type DepthLevel = { px: number; qty: number };
export type Depth = { bids: DepthLevel[]; asks: DepthLevel[] };
export type Trade = { ts: number; px: number; qty: number; aggressor: 'B' | 'S' };
export type Latency = {
  p50Ns: number;
  p99Ns: number;
  p999Ns: number;
  maxNs: number;
  samples: number;
  hist: number[];
};

// Four-state connection state machine (unchanged from v1; see
// docs/design/wireframes.md "Connection state surface integration").
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

export function decodeTopOfBook(msg: ServerMessage): TopOfBook {
  const t = msg.tob;
  return {
    bidPx: t.bid_px === -1 ? null : t.bid_px,
    bidQty: t.bid_qty,
    askPx: t.ask_px === -1 ? null : t.ask_px,
    askQty: t.ask_qty,
    ts: msg.ts,
  };
}

export function decodeDepth(msg: ServerMessage): Depth {
  return {
    bids: msg.depth.bids.map(([px, qty]) => ({ px, qty })),
    asks: msg.depth.asks.map(([px, qty]) => ({ px, qty })),
  };
}

export function decodeTrades(msg: ServerMessage): Trade[] {
  return msg.trades.map((t) => ({
    ts: t.ts,
    px: t.px,
    qty: t.qty,
    aggressor: t.aggressor,
  }));
}

export function decodeLatency(msg: ServerMessage): Latency {
  return {
    p50Ns: msg.latency.p50_ns,
    p99Ns: msg.latency.p99_ns,
    p999Ns: msg.latency.p999_ns,
    maxNs: msg.latency.max_ns,
    samples: msg.latency.samples,
    hist: msg.latency.hist,
  };
}
