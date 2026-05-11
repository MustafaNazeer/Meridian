import { useEffect, useState } from 'react';
import { formatIsoDate, formatWallClock } from '../lib/format';
import { useDashboard } from '../store/dashboard';
import { SYMBOLS, type ConnectionState, type Symbol } from '../types';

function LiveDot({ state }: { state: ConnectionState }) {
  if (state === 'disconnected') {
    return (
      <span
        data-element="LiveDot"
        aria-label="disconnected"
        className="inline-block w-1.5 h-1.5 rounded-full bg-ask"
      />
    );
  }
  if (state === 'stalled') {
    return (
      <span
        data-element="LiveDot"
        aria-label="stalled"
        className="inline-block w-1.5 h-1.5 rounded-full bg-gold-deep"
      />
    );
  }
  return (
    <span
      data-element="LiveDot"
      aria-label="live"
      className="inline-block w-1.5 h-1.5 rounded-full bg-gold shadow-glow-gold animate-pulse-live"
    />
  );
}

function HeaderMetaTail({
  state,
  lastEventTs,
  lastMessageAtMs,
  reconnectInMs,
  nowMs,
}: {
  state: ConnectionState;
  lastEventTs: number;
  lastMessageAtMs: number;
  reconnectInMs: number;
  nowMs: number;
}) {
  if (state === 'connecting') {
    return <span className="text-ink-soft"> engine warming up · Fly cold start</span>;
  }
  if (state === 'stalled') {
    const ageS = Math.max(0, Math.floor((nowMs - lastMessageAtMs) / 1000));
    return <span className="text-ink-soft"> · stalled · last update {ageS}s ago · tick {lastEventTs}</span>;
  }
  if (state === 'disconnected') {
    const inS = Math.max(0, Math.ceil(reconnectInMs / 1000));
    return <span className="text-ask"> · disconnected · reconnecting in {inS}s</span>;
  }
  return <span className="text-ink-soft"> · matching live · tick {lastEventTs}</span>;
}

function SymbolSelector({ active, onPick }: { active: Symbol; onPick: (s: Symbol) => void }) {
  return (
    <div data-element="SymbolSelector" className="flex gap-2">
      {SYMBOLS.map((s) => {
        const isActive = s === active;
        return (
          <button
            key={s}
            type="button"
            onClick={() => onPick(s)}
            data-active={isActive ? 'true' : 'false'}
            className={
              'font-body font-medium uppercase text-meta tracking-aux px-3 py-1.5 border ' +
              (isActive
                ? 'border-gold text-gold'
                : 'border-rule text-ink-soft hover:border-rule-strong')
            }
          >
            {s}
          </button>
        );
      })}
    </div>
  );
}

export function Header() {
  const state = useDashboard((s) => s.connectionState);
  const symbol = useDashboard((s) => s.selectedSymbol);
  const setSymbol = useDashboard((s) => s.setSymbol);
  const lastEventTs = useDashboard((s) => s.wire.lastEventTs);
  const lastMessageAtMs = useDashboard((s) => s.wire.lastMessageAtMs);
  const reconnectInMs = useDashboard((s) => s.reconnectInMs);

  // Wall clock tick at 4 Hz so the header time advances smoothly without
  // forcing a re-render of the whole dashboard.
  const [now, setNow] = useState<Date>(() => new Date());
  useEffect(() => {
    const id = window.setInterval(() => setNow(new Date()), 250);
    return () => window.clearInterval(id);
  }, []);

  return (
    <header
      data-component="Header"
      className="flex items-end justify-between gap-12 border-b border-rule px-12 pt-9 pb-6"
    >
      <div data-element="Brand" className="flex items-end gap-5.5">
        <div className="flex items-baseline">
          <span
            data-element="BrandMark"
            className="font-display italic font-medium text-mark tracking-tight-mark text-ink"
          >
            meridian
          </span>
          <span
            aria-hidden="true"
            className="font-display not-italic font-medium text-mark tracking-tight-mark text-gold ml-0.5"
          >
            .
          </span>
        </div>
        <div
          data-element="BrandTag"
          className="font-display italic text-body text-ink-soft border-l border-rule pl-4.5 leading-snug"
        >
          a price-time priority limit order book,
          <br />
          built for the line where bid meets ask
        </div>
      </div>

      <SymbolSelector active={symbol} onPick={setSymbol} />

      <div
        data-element="HeaderMeta"
        className="text-right font-body text-meta text-ink-soft tracking-stat leading-snug"
      >
        <div className="flex items-center justify-end gap-2">
          <LiveDot state={state} />
          <span className="num text-ink">{formatWallClock(now)} ET</span>
          <HeaderMetaTail
            state={state}
            lastEventTs={lastEventTs}
            lastMessageAtMs={lastMessageAtMs}
            reconnectInMs={reconnectInMs}
            nowMs={now.getTime()}
          />
        </div>
        <div className="text-ink-faint">
          <span className="num text-ink">{formatIsoDate(now)}</span> · synthetic NASDAQ-style mix · single-thread engine
        </div>
      </div>
    </header>
  );
}
