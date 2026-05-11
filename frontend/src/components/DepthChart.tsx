import { formatPrice, formatQty } from '../lib/format';
import { useDashboard } from '../store/dashboard';
import { SectionHead } from './SectionHead';

// L1-only depth visualization: a single bid block and a single ask
// block, sized proportionally, with the mid line in the center. The
// section aux makes it clear this is the v1 view until the extended
// snapshot payload lands.
export function DepthChart() {
  const top = useDashboard((s) => s.displayedTop);
  const state = useDashboard((s) => s.connectionState);
  const ready = state !== 'connecting' && top !== null;

  const bidQty = top?.bidQty ?? 0;
  const askQty = top?.askQty ?? 0;
  const maxQty = Math.max(bidQty, askQty, 1);
  const bidWidth = ready && top!.bidPx !== null ? (bidQty / maxQty) * 240 : 0;
  const askWidth = ready && top!.askPx !== null ? (askQty / maxQty) * 240 : 0;
  const bidPx = top?.bidPx ?? null;
  const askPx = top?.askPx ?? null;
  const mid = bidPx !== null && askPx !== null ? (bidPx + askPx) / 2 : null;
  const imbalance =
    bidQty + askQty > 0 ? ((bidQty - askQty) / (bidQty + askQty)) * 100 : 0;
  const pressure = imbalance > 0 ? 'buying pressure' : 'selling pressure';
  const pressureClass = imbalance > 0 ? 'text-bid' : 'text-ask';

  return (
    <section data-component="DepthChart" className="mb-9">
      <SectionHead
        title="cumulative depth"
        aux="L1 today · 10 levels each side with extended snapshot"
      />
      <svg
        data-element="DepthSVG"
        viewBox="0 0 600 240"
        preserveAspectRatio="none"
        width="100%"
        height="240"
        role="img"
        aria-label="L1 cumulative depth, bid on the left of mid, ask on the right of mid"
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
        {/* grid lines */}
        {[48, 96, 144, 192].map((y) => (
          <line key={y} x1="0" x2="600" y1={y} y2={y} stroke="#232847" strokeWidth="0.5" />
        ))}
        {/* mid line */}
        <line
          x1="300"
          x2="300"
          y1="0"
          y2="240"
          stroke="#D4A24C"
          strokeWidth="0.75"
          strokeDasharray="4 4"
          opacity="0.7"
        />
        <text
          x="306"
          y="16"
          fill="#D4A24C"
          fontFamily="Newsreader, serif"
          fontStyle="italic"
          fontSize="12"
        >
          mid {mid !== null ? formatPrice(mid) : '—'}
        </text>
        {/* bid block: extends leftward from mid */}
        {ready && bidWidth > 0 && (
          <rect
            x={300 - bidWidth}
            y={240 - bidWidth * 0.7}
            width={bidWidth}
            height={bidWidth * 0.7}
            fill="url(#bidGrad)"
            stroke="#4FA89E"
            strokeWidth="1.5"
          />
        )}
        {/* ask block: extends rightward from mid */}
        {ready && askWidth > 0 && (
          <rect
            x={300}
            y={240 - askWidth * 0.7}
            width={askWidth}
            height={askWidth * 0.7}
            fill="url(#askGrad)"
            stroke="#C5765B"
            strokeWidth="1.5"
          />
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
            Bid · <span className="num text-ink-2">{formatQty(bidQty)}</span>
          </span>
        </div>
        <div data-element="LegendAsk" className="flex items-center gap-2">
          <span
            className="inline-block w-3 h-0.5 shadow-glow-ask bg-ask"
            aria-hidden="true"
          />
          <span>
            Ask · <span className="num text-ink-2">{formatQty(askQty)}</span>
          </span>
        </div>
        <div data-element="Imbalance" className="ml-auto">
          <span>imbalance · </span>
          <span className={`num font-medium ${pressureClass}`}>{imbalance.toFixed(1)}%</span>
          <span> {pressure}</span>
        </div>
      </div>
    </section>
  );
}
