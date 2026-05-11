import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import type { ConnectionState, ServerMessage } from '../types';
import {
  BACKOFF_INITIAL_MS,
  BACKOFF_MAX_MS,
  MeridianWsClient,
  STALL_AFTER_MS,
  type SocketLike,
} from './client';

type Listener = (ev: unknown) => void;

class FakeSocket implements SocketLike {
  readyState = 0;
  private listeners = new Map<string, Listener[]>();
  // Track the most recent fake instance for the test to drive.
  static last: FakeSocket | null = null;

  constructor() {
    FakeSocket.last = this;
  }

  addEventListener(type: string, listener: Listener): void {
    const list = this.listeners.get(type) ?? [];
    list.push(listener);
    this.listeners.set(type, list);
  }

  close(): void {
    this.dispatch('close', {});
  }

  open(): void {
    this.readyState = 1;
    this.dispatch('open', {});
  }

  message(msg: ServerMessage): void {
    this.dispatch('message', { data: JSON.stringify(msg) });
  }

  serverClose(): void {
    this.dispatch('close', {});
  }

  private dispatch(type: string, ev: unknown): void {
    const list = this.listeners.get(type);
    if (!list) return;
    for (const l of list) l(ev);
  }
}

function snapshot(ts = 1): ServerMessage {
  return { kind: 'snapshot', bid_px: 100, bid_qty: 5, ask_px: 101, ask_qty: 4, ts };
}

function delta(ts: number): ServerMessage {
  return { kind: 'delta', bid_px: 100, bid_qty: 6, ask_px: 101, ask_qty: 4, ts };
}

describe('MeridianWsClient', () => {
  beforeEach(() => {
    vi.useFakeTimers();
    FakeSocket.last = null;
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('starts in connecting and flips to live on first snapshot', () => {
    const states: ConnectionState[] = [];
    const client = new MeridianWsClient(
      'ws://test/ws',
      {
        onState: (s) => states.push(s),
        onSnapshot: () => undefined,
        onDelta: () => undefined,
        onPerfTick: () => undefined,
        onReconnect: () => undefined,
      },
      () => new FakeSocket(),
    );
    client.start();
    expect(client.currentState()).toBe('connecting');
    // The initial 'connecting' is implicit (the client constructs in it
    // and listeners observe transitions only). The first emitted state
    // is 'live' after the first snapshot.
    expect(states).toEqual([]);

    FakeSocket.last!.open();
    FakeSocket.last!.message(snapshot());

    expect(client.currentState()).toBe('live');
    expect(states).toEqual(['live']);
    client.stop();
  });

  it('transitions live -> stalled after STALL_AFTER_MS with no message', () => {
    const states: ConnectionState[] = [];
    const client = new MeridianWsClient(
      'ws://test/ws',
      {
        onState: (s) => states.push(s),
        onSnapshot: () => undefined,
        onDelta: () => undefined,
        onPerfTick: () => undefined,
        onReconnect: () => undefined,
      },
      () => new FakeSocket(),
    );
    client.start();
    FakeSocket.last!.open();
    FakeSocket.last!.message(snapshot());
    expect(client.currentState()).toBe('live');

    vi.advanceTimersByTime(STALL_AFTER_MS + 1);
    expect(client.currentState()).toBe('stalled');
    client.stop();
  });

  it('returns to live on a subsequent message after stalling', () => {
    const client = new MeridianWsClient(
      'ws://test/ws',
      {
        onState: () => undefined,
        onSnapshot: () => undefined,
        onDelta: () => undefined,
        onPerfTick: () => undefined,
        onReconnect: () => undefined,
      },
      () => new FakeSocket(),
    );
    client.start();
    FakeSocket.last!.open();
    FakeSocket.last!.message(snapshot(1));
    vi.advanceTimersByTime(STALL_AFTER_MS + 1);
    expect(client.currentState()).toBe('stalled');
    FakeSocket.last!.message(delta(2));
    expect(client.currentState()).toBe('live');
    client.stop();
  });

  it('emits exponential backoff on close and caps at BACKOFF_MAX_MS', () => {
    const recon: Array<{ attempt: number; inMs: number }> = [];
    const client = new MeridianWsClient(
      'ws://test/ws',
      {
        onState: () => undefined,
        onSnapshot: () => undefined,
        onDelta: () => undefined,
        onPerfTick: () => undefined,
        onReconnect: (attempt, inMs) => recon.push({ attempt, inMs }),
      },
      () => new FakeSocket(),
    );
    client.start();
    FakeSocket.last!.open();
    // Five rapid close events: each one should schedule the next
    // reconnect with progressively larger but capped delay.
    for (let i = 0; i < 5; i++) {
      FakeSocket.last!.serverClose();
      vi.advanceTimersByTime(BACKOFF_MAX_MS + 1);
    }
    expect(recon[0]?.inMs).toBe(BACKOFF_INITIAL_MS);
    expect(recon[1]?.inMs).toBe(BACKOFF_INITIAL_MS * 2);
    expect(recon[2]?.inMs).toBe(BACKOFF_INITIAL_MS * 4);
    expect(recon[3]?.inMs).toBe(BACKOFF_INITIAL_MS * 8);
    expect(recon[4]?.inMs).toBe(BACKOFF_MAX_MS); // capped (would be 8000 already, so still 8000)
    client.stop();
  });

  it('forwards snapshot and delta payloads through the callbacks with byte counts', () => {
    const snaps: number[] = [];
    const deltas: number[] = [];
    const client = new MeridianWsClient(
      'ws://test/ws',
      {
        onState: () => undefined,
        onSnapshot: (_msg, bytes) => snaps.push(bytes),
        onDelta: (_msg, bytes) => deltas.push(bytes),
        onPerfTick: () => undefined,
        onReconnect: () => undefined,
      },
      () => new FakeSocket(),
    );
    client.start();
    FakeSocket.last!.open();
    FakeSocket.last!.message(snapshot());
    FakeSocket.last!.message(delta(2));
    FakeSocket.last!.message(delta(3));
    expect(snaps.length).toBe(1);
    expect(deltas.length).toBe(2);
    expect(snaps[0]).toBeGreaterThan(0);
    expect(deltas[0]).toBeGreaterThan(0);
    client.stop();
  });

  it('emits a perf tick after PERF_TICK_MS with delta and byte counts for the window', () => {
    const ticks: Array<{ d: number; b: number }> = [];
    const client = new MeridianWsClient(
      'ws://test/ws',
      {
        onState: () => undefined,
        onSnapshot: () => undefined,
        onDelta: () => undefined,
        onPerfTick: (d, b) => ticks.push({ d, b }),
        onReconnect: () => undefined,
      },
      () => new FakeSocket(),
    );
    client.start();
    FakeSocket.last!.open();
    FakeSocket.last!.message(snapshot());
    FakeSocket.last!.message(delta(2));
    FakeSocket.last!.message(delta(3));
    vi.advanceTimersByTime(1_000);
    expect(ticks.length).toBe(1);
    expect(ticks[0].d).toBe(3); // 1 snapshot + 2 deltas
    expect(ticks[0].b).toBeGreaterThan(0);
    // The window resets after a tick:
    vi.advanceTimersByTime(1_000);
    expect(ticks[1].d).toBe(0);
    expect(ticks[1].b).toBe(0);
    client.stop();
  });

  it('does not reconnect after stop()', () => {
    const recon: number[] = [];
    const client = new MeridianWsClient(
      'ws://test/ws',
      {
        onState: () => undefined,
        onSnapshot: () => undefined,
        onDelta: () => undefined,
        onPerfTick: () => undefined,
        onReconnect: (attempt) => recon.push(attempt),
      },
      () => new FakeSocket(),
    );
    client.start();
    FakeSocket.last!.open();
    client.stop();
    FakeSocket.last!.serverClose();
    vi.advanceTimersByTime(BACKOFF_MAX_MS * 2);
    expect(recon.length).toBe(0);
  });
});
