import { formatPrice, formatQty, formatSpread, formatThousands } from '../lib/format';
import { symbolMeta } from '../lib/symbols';
import { useDashboard } from '../store/dashboard';

function Skeleton({ w = 80, h = 32 }: { w?: number; h?: number }) {
  return (
    <span
      data-element="Skeleton"
      className="inline-block bg-surface rounded-bar"
      style={{ width: `${w}px`, height: `${h}px` }}
    />
  );
}

function StatLast() {
  const top = useDashboard((s) => s.top);
  const color = useDashboard((s) => s.lastTickColor);
  const ready = useDashboard((s) => s.connectionState !== 'connecting');
  const bidPx = top?.bidPx ?? null;
  const askPx = top?.askPx ?? null;
  const last =
    bidPx !== null && askPx !== null ? (bidPx + askPx) / 2 : null;
  const colorClass =
    color === 'up' ? 'text-bid' : color === 'down' ? 'text-ask' : 'text-ink';

  return (
    <div data-element="StatLast" className="border-l border-rule pl-5.5">
      <div className="font-display italic text-meta text-ink-soft tracking-stat mb-2">
        Last (mid)
      </div>
      {ready && last !== null ? (
        <>
          <div
            data-element="StatValue"
            className={`num font-light text-display-stat ${colorClass} leading-1 tracking-tight-display`}
          >
            {formatPrice(last)}
          </div>
          <div className="font-body text-trades text-ink-soft mt-1.5">
            <span className="num text-ink-2">
              bid {formatPrice(bidPx)} / ask {formatPrice(askPx)}
            </span>
          </div>
        </>
      ) : (
        <>
          <Skeleton />
          <div className="mt-1.5">
            <Skeleton w={120} h={12} />
          </div>
        </>
      )}
    </div>
  );
}

function StatSpread() {
  const top = useDashboard((s) => s.top);
  const ready = useDashboard((s) => s.connectionState !== 'connecting');
  const s = formatSpread(top?.bidPx ?? null, top?.askPx ?? null);
  return (
    <div data-element="StatSpread" className="border-l border-rule pl-5.5">
      <div className="font-display italic text-meta text-ink-soft tracking-stat mb-2">
        Spread
      </div>
      {ready && top !== null && top.bidPx !== null && top.askPx !== null ? (
        <>
          <div className="num font-light text-display-stat text-gold leading-1 tracking-tight-display">
            {s.abs}
          </div>
          <div className="font-body text-trades text-ink-soft mt-1.5">
            <span className="num text-ink-2">{s.bps}</span> bp · {s.band}
          </div>
        </>
      ) : (
        <>
          <Skeleton />
          <div className="mt-1.5">
            <Skeleton w={120} h={12} />
          </div>
        </>
      )}
    </div>
  );
}

function StatBookSize() {
  const top = useDashboard((s) => s.top);
  const ticks = useDashboard((s) => s.ticksSinceConnect);
  const ready = useDashboard((s) => s.connectionState !== 'connecting');
  const totalQty = (top?.bidQty ?? 0) + (top?.askQty ?? 0);
  return (
    <div data-element="StatBookSize" className="border-l border-rule pl-5.5">
      <div className="font-display italic text-meta text-ink-soft tracking-stat mb-2">
        Top size
      </div>
      {ready && top !== null ? (
        <>
          <div className="num font-light text-display-stat text-ink leading-1 tracking-tight-display">
            {formatQty(totalQty)}
          </div>
          <div className="font-body text-trades text-ink-soft mt-1.5">
            bid <span className="num text-ink-2">{formatQty(top.bidQty)}</span>
            <span> · </span>
            ask <span className="num text-ink-2">{formatQty(top.askQty)}</span>
            <span> · </span>
            <span className="num text-ink-2">{formatThousands(ticks)}</span> ticks
          </div>
        </>
      ) : (
        <>
          <Skeleton />
          <div className="mt-1.5">
            <Skeleton w={140} h={12} />
          </div>
        </>
      )}
    </div>
  );
}

function HeroIdentity() {
  const state = useDashboard((s) => s.connectionState);
  const symbol = useDashboard((s) => s.selectedSymbol);
  const ticks = useDashboard((s) => s.ticksSinceConnect);
  const top = useDashboard((s) => s.top);
  const meta = symbolMeta(symbol);

  if (state === 'connecting') {
    return (
      <div data-element="HeroWarmingUp">
        <div className="font-display italic text-meta text-ink-soft tracking-stat mb-3">
          tracking · synthetic NMS Tier 1 · {meta.longName} · {meta.market}
        </div>
        <div className="font-display italic font-medium text-ink leading-1 tracking-tight-display" style={{ fontSize: '56px' }}>
          warming.
        </div>
        <div className="font-body text-trades text-ink-soft mt-3">
          engine event stream connecting · this takes a moment on a fresh socket
        </div>
        <div className="mt-3 flex items-center gap-1.5">
          <span className="inline-block w-1.5 h-1.5 rounded-full bg-gold shadow-glow-gold animate-pulse-live" />
          <span className="inline-block w-1.5 h-1.5 rounded-full bg-gold shadow-glow-gold animate-pulse-live" style={{ animationDelay: '200ms' }} />
          <span className="inline-block w-1.5 h-1.5 rounded-full bg-gold shadow-glow-gold animate-pulse-live" style={{ animationDelay: '400ms' }} />
        </div>
      </div>
    );
  }

  return (
    <div data-element="HeroIdentity">
      <div
        data-element="HeroLede"
        className="font-display italic text-meta text-ink-soft tracking-stat mb-3"
      >
        tracking · synthetic NMS Tier 1 · {meta.longName} · {meta.market}
      </div>
      <div
        data-element="HeroSymbol"
        className="font-display italic font-medium text-display-xl text-ink leading-1 tracking-tight-display"
      >
        {symbol}
      </div>
      <div
        data-element="HeroMeta"
        className="font-body text-trades text-ink-soft mt-3"
      >
        <span className="num text-ink-2">{formatQty(top?.bidQty ?? 0)}</span> resting bid ·{' '}
        <span className="num text-ink-2">{formatQty(top?.askQty ?? 0)}</span> resting ask ·{' '}
        <span className="num text-ink-2">{formatThousands(ticks)}</span> ticks since connect
      </div>
    </div>
  );
}

export function Hero() {
  return (
    <section
      data-component="Hero"
      className="grid grid-cols-hero gap-12 items-end border-b border-rule px-12 pt-14 pb-11"
    >
      <HeroIdentity />
      <StatLast />
      <StatSpread />
      <StatBookSize />
    </section>
  );
}
