import type { ConnectionState, ServerMessage } from '../types';

// Configuration knobs. Held as constants rather than constructor
// arguments so the connection state machine has one single source of
// truth for backoff and stall thresholds; tests construct a client with
// an injected socket factory and observe the same behaviour the demo
// runs.
export const STALL_AFTER_MS = 1_000;
export const BACKOFF_INITIAL_MS = 500;
export const BACKOFF_MAX_MS = 8_000;
export const PERF_TICK_MS = 1_000;

export type WsCallbacks = {
  onState: (s: ConnectionState) => void;
  onSnapshot: (msg: ServerMessage, bytes: number) => void;
  onDelta: (msg: ServerMessage, bytes: number) => void;
  onPerfTick: (deltas: number, bytes: number) => void;
  onReconnect: (attempt: number, inMs: number) => void;
};

// Minimal abstraction over WebSocket for testability. Real callers pass
// the global `WebSocket` constructor; tests pass a fake whose lifecycle
// (open / message / close / error) they drive synchronously.
export interface SocketLike {
  readonly readyState: number;
  close(code?: number, reason?: string): void;
  addEventListener<K extends 'open' | 'message' | 'close' | 'error'>(
    type: K,
    listener: (ev: SocketEventMap[K]) => void,
  ): void;
}

type SocketEventMap = {
  open: Event;
  message: MessageEvent<string>;
  close: CloseEvent;
  error: Event;
};

export type SocketFactory = (url: string) => SocketLike;

const defaultFactory: SocketFactory = (url) =>
  new WebSocket(url) as unknown as SocketLike;

export class MeridianWsClient {
  private url: string;
  private factory: SocketFactory;
  private cb: WsCallbacks;
  private socket: SocketLike | null = null;
  private state: ConnectionState = 'connecting';
  private stallTimer: ReturnType<typeof setTimeout> | null = null;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private perfTimer: ReturnType<typeof setInterval> | null = null;
  private attempt = 0;
  private windowDeltas = 0;
  private windowBytes = 0;
  private stopped = false;

  constructor(url: string, cb: WsCallbacks, factory: SocketFactory = defaultFactory) {
    this.url = url;
    this.cb = cb;
    this.factory = factory;
  }

  start(): void {
    this.stopped = false;
    this.connect();
    this.perfTimer = setInterval(() => this.flushPerf(), PERF_TICK_MS);
  }

  stop(): void {
    this.stopped = true;
    this.clearStallTimer();
    this.clearReconnectTimer();
    if (this.perfTimer !== null) {
      clearInterval(this.perfTimer);
      this.perfTimer = null;
    }
    if (this.socket !== null) {
      try {
        this.socket.close(1000, 'client stop');
      } catch {
        // ignore close errors during teardown
      }
      this.socket = null;
    }
  }

  // Diagnostic getters for tests.
  currentState(): ConnectionState {
    return this.state;
  }

  attempts(): number {
    return this.attempt;
  }

  private connect(): void {
    this.setState('connecting');
    const socket = this.factory(this.url);
    this.socket = socket;
    socket.addEventListener('open', () => {
      this.attempt = 0;
      this.armStallTimer();
    });
    socket.addEventListener('message', (ev) => this.onMessage(ev));
    socket.addEventListener('close', () => this.onClose());
    socket.addEventListener('error', () => {
      // Errors precede a close; let close handle the reconnect.
    });
  }

  private onMessage(ev: MessageEvent<string>): void {
    const payload = ev.data;
    const bytes = typeof payload === 'string' ? payload.length : 0;
    let msg: ServerMessage;
    try {
      msg = JSON.parse(payload) as ServerMessage;
    } catch {
      return; // ignore malformed payloads; the server is the source of truth
    }
    if (msg.kind === 'snapshot') {
      this.cb.onSnapshot(msg, bytes);
    } else if (msg.kind === 'delta') {
      this.cb.onDelta(msg, bytes);
    } else {
      return; // unknown kind; ignore
    }
    this.windowDeltas += 1;
    this.windowBytes += bytes;
    this.setState('live');
    this.armStallTimer();
  }

  private onClose(): void {
    this.clearStallTimer();
    this.socket = null;
    if (this.stopped) return;
    this.attempt += 1;
    const wait = Math.min(
      BACKOFF_INITIAL_MS * 2 ** Math.max(0, this.attempt - 1),
      BACKOFF_MAX_MS,
    );
    this.setState('disconnected');
    this.cb.onReconnect(this.attempt, wait);
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      if (!this.stopped) this.connect();
    }, wait);
  }

  private armStallTimer(): void {
    this.clearStallTimer();
    this.stallTimer = setTimeout(() => {
      this.stallTimer = null;
      if (this.state === 'live') this.setState('stalled');
    }, STALL_AFTER_MS);
  }

  private clearStallTimer(): void {
    if (this.stallTimer !== null) {
      clearTimeout(this.stallTimer);
      this.stallTimer = null;
    }
  }

  private clearReconnectTimer(): void {
    if (this.reconnectTimer !== null) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
  }

  private setState(s: ConnectionState): void {
    if (this.state === s) return;
    this.state = s;
    this.cb.onState(s);
  }

  private flushPerf(): void {
    const d = this.windowDeltas;
    const b = this.windowBytes;
    this.windowDeltas = 0;
    this.windowBytes = 0;
    this.cb.onPerfTick(d, b);
  }
}
