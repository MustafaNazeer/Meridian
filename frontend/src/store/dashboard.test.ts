import { describe, expect, it, beforeEach } from 'vitest';
import { useDashboard } from './dashboard';
import type { Depth, Latency, TopOfBook, Trade } from '../types';

function tob(bidPx: number | null, askPx: number | null, ts: number): TopOfBook {
  return {
    bidPx,
    bidQty: bidPx === null ? 0 : 10,
    askPx,
    askQty: askPx === null ? 0 : 8,
    ts,
  };
}

function emptyDepth(): Depth {
  return { bids: [], asks: [] };
}
function emptyTrades(): Trade[] {
  return [];
}
function emptyLatency(): Latency {
  return {
    p50Ns: 0, p99Ns: 0, p999Ns: 0, maxNs: 0,
    samples: 0, hist: Array(33).fill(0),
  };
}

describe('useDashboard', () => {
  beforeEach(() => {
    useDashboard.getState().reset();
  });

  it('starts in connecting with empty state', () => {
    const s = useDashboard.getState();
    expect(s.connectionState).toBe('connecting');
    expect(s.top).toBeNull();
    expect(s.ticksSinceConnect).toBe(0);
    expect(s.history.length).toBe(0);
  });

  it('applies a snapshot, flips to live, and seeds history', () => {
    useDashboard.getState().applySnapshot(tob(100, 101, 1), 64, emptyDepth(), emptyTrades(), emptyLatency());
    const s = useDashboard.getState();
    expect(s.connectionState).toBe('live');
    expect(s.top).toEqual(tob(100, 101, 1));
    expect(s.ticksSinceConnect).toBe(1);
    expect(s.history.length).toBe(1);
    expect(s.history[0].px).toBeCloseTo(100.5);
  });

  it('colors deltas up when mid rises and down when mid falls', () => {
    useDashboard.getState().applySnapshot(tob(100, 101, 1), 0, emptyDepth(), emptyTrades(), emptyLatency());
    expect(useDashboard.getState().lastTickColor).toBe('flat');
    useDashboard.getState().applyDelta(tob(101, 102, 2), 0, emptyDepth(), emptyTrades(), emptyLatency());
    expect(useDashboard.getState().lastTickColor).toBe('up');
    useDashboard.getState().applyDelta(tob(99, 100, 3), 0, emptyDepth(), emptyTrades(), emptyLatency());
    expect(useDashboard.getState().lastTickColor).toBe('down');
  });

  it('keeps the prior color when mid is unchanged', () => {
    useDashboard.getState().applySnapshot(tob(100, 101, 1), 0, emptyDepth(), emptyTrades(), emptyLatency());
    useDashboard.getState().applyDelta(tob(101, 102, 2), 0, emptyDepth(), emptyTrades(), emptyLatency());
    expect(useDashboard.getState().lastTickColor).toBe('up');
    useDashboard.getState().applyDelta(tob(101, 102, 3), 0, emptyDepth(), emptyTrades(), emptyLatency());
    expect(useDashboard.getState().lastTickColor).toBe('up');
  });

  it('decodes empty sides as a null mid and history is not appended', () => {
    useDashboard.getState().applySnapshot(tob(null, 101, 1), 0, emptyDepth(), emptyTrades(), emptyLatency());
    const s = useDashboard.getState();
    expect(s.top?.bidPx).toBeNull();
    expect(s.history.length).toBe(0);
  });

  it('symbol change resets state and re-enters connecting', () => {
    useDashboard.getState().applySnapshot(tob(100, 101, 1), 0, emptyDepth(), emptyTrades(), emptyLatency());
    useDashboard.getState().setSymbol('NVDA');
    const s = useDashboard.getState();
    expect(s.selectedSymbol).toBe('NVDA');
    expect(s.top).toBeNull();
    expect(s.ticksSinceConnect).toBe(0);
    expect(s.connectionState).toBe('connecting');
  });

  it('records wire byte/delta totals', () => {
    useDashboard.getState().applySnapshot(tob(100, 101, 1), 50, emptyDepth(), emptyTrades(), emptyLatency());
    useDashboard.getState().applyDelta(tob(101, 102, 2), 30, emptyDepth(), emptyTrades(), emptyLatency());
    useDashboard.getState().applyDelta(tob(102, 103, 3), 30, emptyDepth(), emptyTrades(), emptyLatency());
    const s = useDashboard.getState();
    expect(s.wire.totalDeltas).toBe(3);
    expect(s.wire.totalBytes).toBe(110);
    expect(s.wire.lastEventTs).toBe(3);
  });

  it('setReconnect drops connection state to disconnected', () => {
    useDashboard.getState().applySnapshot(tob(100, 101, 1), 0, emptyDepth(), emptyTrades(), emptyLatency());
    useDashboard.getState().setReconnect(3, 2_000);
    const s = useDashboard.getState();
    expect(s.connectionState).toBe('disconnected');
    expect(s.reconnectAttempt).toBe(3);
    expect(s.reconnectInMs).toBe(2_000);
  });

  it('a snapshot eagerly populates displayedTop so the dashboard leaves the connecting skeleton immediately', () => {
    useDashboard.getState().applySnapshot(tob(10000, 10001, 1), 0, emptyDepth(), emptyTrades(), emptyLatency());
    const s = useDashboard.getState();
    expect(s.displayedTop).not.toBeNull();
    expect(s.displayedTop?.bidPx).toBe(10000);
    expect(s.displayedTop?.askPx).toBe(10001);
  });

  it('deltas update top but leave displayedTop alone until flushDisplayed is called', () => {
    useDashboard.getState().applySnapshot(tob(10000, 10001, 1), 0, emptyDepth(), emptyTrades(), emptyLatency());
    const initial = useDashboard.getState().displayedTop;
    useDashboard.getState().applyDelta(tob(10001, 10002, 2), 0, emptyDepth(), emptyTrades(), emptyLatency());
    useDashboard.getState().applyDelta(tob(10002, 10003, 3), 0, emptyDepth(), emptyTrades(), emptyLatency());
    const afterDeltas = useDashboard.getState();
    expect(afterDeltas.top?.ts).toBe(3);
    // displayedTop still references the snapshot, not the latest delta:
    expect(afterDeltas.displayedTop).toBe(initial);

    useDashboard.getState().flushDisplayed();
    const afterFlush = useDashboard.getState();
    expect(afterFlush.displayedTop?.ts).toBe(3);
  });

  it('flushDisplayed is a no-op when top has not changed since the last flush', () => {
    useDashboard.getState().applySnapshot(tob(10000, 10001, 1), 0, emptyDepth(), emptyTrades(), emptyLatency());
    const ref = useDashboard.getState().displayedTop;
    useDashboard.getState().flushDisplayed();
    expect(useDashboard.getState().displayedTop).toBe(ref);
  });

  it('applySnapshot writes top, depth, trades, latency together', () => {
    const depth: Depth = { bids: [{ px: 100, qty: 10 }, { px: 99, qty: 5 }], asks: [{ px: 101, qty: 8 }] };
    const trades: Trade[] = [{ ts: 1, px: 100, qty: 2, aggressor: 'B' }];
    const latency: Latency = { p50Ns: 100, p99Ns: 500, p999Ns: 1200, maxNs: 5000, samples: 1234, hist: Array(33).fill(0) };
    useDashboard.getState().applySnapshot(tob(100, 101, 1), 0, depth, trades, latency);
    const s = useDashboard.getState();
    expect(s.depth).toEqual(depth);
    expect(s.displayedDepth).toEqual(depth);
    expect(s.trades).toEqual(trades);
    expect(s.latency).toEqual(latency);
    expect(s.displayedLatency).toEqual(latency);
  });

  it('flushDisplayed mirrors top, depth, latency atomically', () => {
    const d0: Depth = { bids: [{ px: 100, qty: 10 }], asks: [{ px: 101, qty: 8 }] };
    const l0: Latency = { p50Ns: 100, p99Ns: 500, p999Ns: 1200, maxNs: 5000, samples: 1, hist: Array(33).fill(0) };
    useDashboard.getState().applySnapshot(tob(100, 101, 1), 0, d0, [], l0);
    const d1: Depth = { bids: [{ px: 99, qty: 11 }], asks: [{ px: 102, qty: 9 }] };
    const l1: Latency = { p50Ns: 150, p99Ns: 600, p999Ns: 1300, maxNs: 6000, samples: 2, hist: Array(33).fill(0) };
    useDashboard.getState().applyDelta(tob(99, 102, 2), 0, d1, [], l1);
    expect(useDashboard.getState().top?.bidPx).toBe(99);
    expect(useDashboard.getState().displayedTop?.bidPx).toBe(100);
    expect(useDashboard.getState().displayedDepth).toEqual(d0);
    expect(useDashboard.getState().displayedLatency).toEqual(l0);
    useDashboard.getState().flushDisplayed();
    expect(useDashboard.getState().displayedTop?.bidPx).toBe(99);
    expect(useDashboard.getState().displayedDepth).toEqual(d1);
    expect(useDashboard.getState().displayedLatency).toEqual(l1);
  });

  it('trades field updates each delta with the latest window', () => {
    const empty = emptyLatency();
    const t1: Trade[] = [{ ts: 1, px: 100, qty: 1, aggressor: 'B' }];
    useDashboard.getState().applySnapshot(tob(100, 101, 1), 0, emptyDepth(), t1, empty);
    expect(useDashboard.getState().trades).toEqual(t1);
    const t2: Trade[] = [
      { ts: 1, px: 100, qty: 1, aggressor: 'B' },
      { ts: 2, px: 101, qty: 1, aggressor: 'S' },
    ];
    useDashboard.getState().applyDelta(tob(101, 102, 2), 0, emptyDepth(), t2, empty);
    expect(useDashboard.getState().trades).toEqual(t2);
  });
});
