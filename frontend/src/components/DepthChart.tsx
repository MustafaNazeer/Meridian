import { formatPrice, formatQty } from '../lib/format';
import { useDashboard } from '../store/dashboard';
import { SectionHead } from './SectionHead';

const VIEW_W = 600;
const VIEW_H = 240;
const HALF_W = VIEW_W / 2;

function buildPath(
  levels: { px: number; qty: number }[],
  side: 'bid' | 'ask',
  maxCum: number,
  priceSpan: number,
  midPx: number,
): string {
  if (levels.length === 0 || maxCum <= 0) return '';
  const cums: number[] = [];
  let cum = 0;
  for (const l of levels) { cum += l.qty; cums.push(cum); }
  const cumScale = (c: number) => VIEW_H - (c / maxCum) * (VIEW_H - 16);
  const xFromPx = (px: number) => {
    const dx = ((Math.abs(px - midPx)) / priceSpan) * HALF_W;
    return side === 'bid' ? HALF_W - dx : HALF_W + dx;
  };
  let d = `M ${HALF_W} ${VIEW_H}`;
  for (let i = 0; i < levels.length; i++) {
    const x = xFromPx(levels[i].px);
    const y = cumScale(cums[i]);
    d += ` L ${x} ${VIEW_H} L ${x} ${y}`;
  }
  const lastX = xFromPx(levels[levels.length - 1].px);
  d += ` L ${lastX} ${VIEW_H} Z`;
  return d;
}

export function DepthChart() {
  const depth = useDashboard((s) => s.displayedDepth);
  const top = useDashboard((s) => s.displayedTop);
  const state = useDashboard((s) => s.connectionState);
  const ready = state !== 'connecting' && depth !== null && top !== null;

  const bids = depth?.bids ?? [];
  const asks = depth?.asks ?? [];
  const bidCum = bids.reduce((a, l) => a + l.qty, 0);
  const askCum = asks.reduce((a, l) => a + l.qty, 0);
  const maxCum = Math.max(bidCum, askCum, 1);

  const bidPx = top?.bidPx ?? null;
  const askPx = top?.askPx ?? null;
  const mid = bidPx !== null && askPx !== null ? (bidPx + askPx) / 2 : null;
  const deepestBid = bids.length > 0 ? bids[bids.length - 1].px : (bidPx ?? 0);
  const deepestAsk = asks.length > 0 ? asks[asks.length - 1].px : (askPx ?? 0);
  const priceSpan =
    mid === null ? 1 : Math.max(mid - deepestBid, deepestAsk - mid, 1);

  const bidPath = ready && mid !== null ? buildPath(bids, 'bid', maxCum, priceSpan, mid) : '';
  const askPath = ready && mid !== null ? buildPath(asks, 'ask', maxCum, priceSpan, mid) : '';

  const imbalance =
    bidCum + askCum > 0 ? ((bidCum - askCum) / (bidCum + askCum)) * 100 : 0;
  const pressure = imbalance > 0 ? 'buying pressure' : 'selling pressure';
  const pressureClass = imbalance > 0 ? 'text-bid' : 'text-ask';

  return (
    <section data-component="DepthChart" className="mb-9">
      <SectionHead title="cumulative depth" aux="8 levels each side, cumulative" />
      <svg
        data-element="DepthSVG"
        viewBox={`0 0 ${VIEW_W} ${VIEW_H}`}
        preserveAspectRatio="none"
        width="100%"
        height={String(VIEW_H)}
        role="img"
        aria-label="L8 cumulative depth, bid side to the left of mid, ask side to the right"
      >
        <defs>
          <linearGradient id="bidGrad" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0%" stopColor="#4FA89E" stopOpacity="0.5" />
            <stop offset="100%" stopColor="#4FA89E" stopOpacity="0.02" />
          </linearGradient>
          <linearGradient id="askGrad" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0%" stopColor="#C5765B" stopOpacity="0.5" />
            <stop offset="100%" stopColor="#C5765B" stopOpacity="0.02" />
          </linearGradient>
        </defs>
        {[48, 96, 144, 192].map((y) => (
          <line key={y} x1="0" x2={VIEW_W} y1={y} y2={y} stroke="#232847" strokeWidth="0.5" />
        ))}
        <line
          x1={HALF_W}
          x2={HALF_W}
          y1="0"
          y2={VIEW_H}
          stroke="#D4A24C"
          strokeWidth="0.75"
          strokeDasharray="4 4"
          opacity="0.7"
        />
        <text
          x={HALF_W + 6}
          y="16"
          fill="#D4A24C"
          fontFamily="Newsreader, serif"
          fontStyle="italic"
          fontSize="12"
        >
          mid {mid !== null ? formatPrice(mid) : '—'}
        </text>
        {bidPath && (
          <path d={bidPath} fill="url(#bidGrad)" stroke="#4FA89E" strokeWidth="1.5" />
        )}
        {askPath && (
          <path d={askPath} fill="url(#askGrad)" stroke="#C5765B" strokeWidth="1.5" />
        )}
      </svg>
      <div
        data-element="ChartAxis"
        className="flex justify-between font-mono text-micro text-ink-faint mt-2"
      >
        <span>{bidPx !== null ? formatPrice(bidPx) : '—'}</span>
        <span>{mid !== null ? formatPrice(mid - 1.5) : ''}</span>
        <span>mid</span>
        <span>{mid !== null ? formatPrice(mid + 1.5) : ''}</span>
        <span>{askPx !== null ? formatPrice(askPx) : '—'}</span>
      </div>
      <div
        data-element="ChartLegend"
        className="flex items-center gap-6 mt-3.5 font-body text-trades text-ink-soft"
      >
        <div data-element="LegendBid" className="flex items-center gap-2">
          <span
            className="inline-block w-3 h-0.5 shadow-glow-bid bg-bid"
            aria-hidden="true"
          />
          <span>
            Bid {'·'} <span className="num text-ink-2">{formatQty(bidCum)}</span>
          </span>
        </div>
        <div data-element="LegendAsk" className="flex items-center gap-2">
          <span
            className="inline-block w-3 h-0.5 shadow-glow-ask bg-ask"
            aria-hidden="true"
          />
          <span>
            Ask {'·'} <span className="num text-ink-2">{formatQty(askCum)}</span>
          </span>
        </div>
        <div data-element="Imbalance" className="ml-auto">
          <span>imbalance {'·'} </span>
          <span className={`num font-medium ${pressureClass}`}>{imbalance.toFixed(1)}%</span>
          <span> {pressure}</span>
        </div>
      </div>
    </section>
  );
}
