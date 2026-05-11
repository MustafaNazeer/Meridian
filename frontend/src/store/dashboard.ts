import { create } from 'zustand';
import type { ConnectionState, Symbol, TopOfBook } from '../types';

// Rolling per-tick price history so the hero "Last" stat can compare
// the latest tick to the prior tick and recolor up/down. The history
// is bounded; only the last 64 ticks are kept.
const HISTORY_MAX = 64;

export type LastTick = {
  px: number;
  ts: number;
};

// Lightweight performance counters derived client-side: how many
// deltas the WS client has seen in the last second, how many bytes
// the WS has carried in the last second, and the moving average of
// each. These are honest measurements of the frontend's view of the
// stream rather than engine-internal metrics (those land with the
// extended snapshot payload in a follow-up milestone).
export type WireCounters = {
  deltasPerSec: number;
  bytesPerSec: number;
  totalDeltas: number;
  totalBytes: number;
  lastEventTs: number; // engine event timestamp from the most recent message
  lastMessageAtMs: number; // host clock wall time of the most recent message
};

export type DashboardState = {
  connectionState: ConnectionState;
  selectedSymbol: Symbol;
  top: TopOfBook | null;
  // `displayedTop` mirrors `top` at a throttled cadence (see
  // hooks/useDisplayThrottle). The editorial cells (Hero stats, Ladder,
  // Tape) read this so their numerals refresh at a human-legible rate;
  // the wire counters and depth-chart inputs continue to read `top` so
  // the perf panel and chart stay accurate.
  displayedTop: TopOfBook | null;
  prevLast: LastTick | null;
  lastTickColor: 'up' | 'down' | 'flat';
  ticksSinceConnect: number;
  history: LastTick[]; // last N midpoints, oldest first
  reconnectAttempt: number; // 0 when connected; increments on each reconnect
  reconnectInMs: number; // when disconnected, ms until the next retry
  wire: WireCounters;

  // Reducers.
  setConnectionState: (s: ConnectionState) => void;
  setSymbol: (s: Symbol) => void;
  applySnapshot: (t: TopOfBook, bytes: number) => void;
  applyDelta: (t: TopOfBook, bytes: number) => void;
  notePerfTick: (perSec: { deltas: number; bytes: number }) => void;
  setReconnect: (attempt: number, inMs: number) => void;
  flushDisplayed: () => void;
  reset: () => void;
};

const initialWire: WireCounters = {
  deltasPerSec: 0,
  bytesPerSec: 0,
  totalDeltas: 0,
  totalBytes: 0,
  lastEventTs: 0,
  lastMessageAtMs: 0,
};

export const useDashboard = create<DashboardState>((set) => ({
  connectionState: 'connecting',
  selectedSymbol: 'AAPL',
  top: null,
  displayedTop: null,
  prevLast: null,
  lastTickColor: 'flat',
  ticksSinceConnect: 0,
  history: [],
  reconnectAttempt: 0,
  reconnectInMs: 0,
  wire: initialWire,

  setConnectionState: (s) => set({ connectionState: s }),

  setSymbol: (s) =>
    set({
      selectedSymbol: s,
      top: null,
      displayedTop: null,
      prevLast: null,
      lastTickColor: 'flat',
      ticksSinceConnect: 0,
      history: [],
      connectionState: 'connecting',
    }),

  applySnapshot: (t, bytes) =>
    set((prev) => {
      const lastMid = midOf(t);
      const history = lastMid !== null ? [{ px: lastMid, ts: t.ts }] : [];
      return {
        top: t,
        // A snapshot is the start of a stream: mirror immediately to
        // displayedTop so the editorial cells leave the connecting
        // skeleton without waiting for the next throttle tick.
        displayedTop: t,
        // A snapshot is the start of a stream; reset prior-tick state.
        prevLast: null,
        lastTickColor: 'flat',
        ticksSinceConnect: 1,
        history,
        connectionState: 'live',
        reconnectAttempt: 0,
        reconnectInMs: 0,
        wire: {
          ...prev.wire,
          totalDeltas: prev.wire.totalDeltas + 1,
          totalBytes: prev.wire.totalBytes + bytes,
          lastEventTs: t.ts,
          lastMessageAtMs: Date.now(),
        },
      };
    }),

  applyDelta: (t, bytes) =>
    set((prev) => {
      const nextMid = midOf(t);
      const prevMid = prev.top ? midOf(prev.top) : null;
      let color: 'up' | 'down' | 'flat' = prev.lastTickColor;
      let nextPrevLast = prev.prevLast;
      if (nextMid !== null && prevMid !== null) {
        if (nextMid > prevMid) color = 'up';
        else if (nextMid < prevMid) color = 'down';
        // equal price keeps prior color (stable visual)
        nextPrevLast = { px: prevMid, ts: prev.top?.ts ?? 0 };
      }
      const history =
        nextMid !== null
          ? appendBounded(prev.history, { px: nextMid, ts: t.ts })
          : prev.history;
      return {
        top: t,
        prevLast: nextPrevLast,
        lastTickColor: color,
        ticksSinceConnect: prev.ticksSinceConnect + 1,
        history,
        connectionState: 'live',
        wire: {
          ...prev.wire,
          totalDeltas: prev.wire.totalDeltas + 1,
          totalBytes: prev.wire.totalBytes + bytes,
          lastEventTs: t.ts,
          lastMessageAtMs: Date.now(),
        },
      };
    }),

  notePerfTick: ({ deltas, bytes }) =>
    set((prev) => ({
      wire: {
        ...prev.wire,
        deltasPerSec: deltas,
        bytesPerSec: bytes,
      },
    })),

  setReconnect: (attempt, inMs) =>
    set({
      reconnectAttempt: attempt,
      reconnectInMs: inMs,
      connectionState: 'disconnected',
    }),

  // Mirror the latest `top` into `displayedTop`. Called from a 4 Hz
  // interval in App.tsx (via useDisplayThrottle), this throttles
  // rendering of the editorial cells without throttling the underlying
  // wire stream.
  flushDisplayed: () =>
    set((prev) => (prev.top === prev.displayedTop ? {} : { displayedTop: prev.top })),

  reset: () =>
    set({
      connectionState: 'connecting',
      top: null,
      displayedTop: null,
      prevLast: null,
      lastTickColor: 'flat',
      ticksSinceConnect: 0,
      history: [],
      reconnectAttempt: 0,
      reconnectInMs: 0,
      wire: initialWire,
    }),
}));

function midOf(t: TopOfBook): number | null {
  if (t.bidPx === null || t.askPx === null) return null;
  return (t.bidPx + t.askPx) / 2;
}

function appendBounded(arr: LastTick[], next: LastTick): LastTick[] {
  if (arr.length < HISTORY_MAX) return [...arr, next];
  return [...arr.slice(arr.length - (HISTORY_MAX - 1)), next];
}
