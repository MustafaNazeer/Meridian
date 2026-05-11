import { formatPrice, formatQty } from '../lib/format';
import { useDashboard } from '../store/dashboard';
import { SectionHead } from './SectionHead';

// 8 rows per side per wireframes; today only the row closest to the
// spread carries real data because the wire payload is TOB-only.
const ROWS_PER_SIDE = 8;

type Row = {
  px: number | null;
  qty: number;
  isReal: boolean;
};

function LadderRow({
  row,
  side,
  cumPct,
}: {
  row: Row;
  side: 'bid' | 'ask';
  cumPct: number;
}) {
  const priceClass = side === 'bid' ? 'text-bid' : 'text-ask';
  const depthClass = side === 'bid' ? 'bg-bid-soft' : 'bg-ask-soft';
  return (
    <tr
      data-element="LadderRow"
      data-side={side}
      className="hover:bg-surface/40"
    >
      <td
        data-element="RowPrice"
        className={`num ${priceClass} px-8 py-1.5 relative`}
      >
        {row.isReal ? formatPrice(row.px) : '—'}
      </td>
      <td
        data-element="RowSize"
        className="num text-ink-2 px-8 py-1.5 text-right relative"
      >
        {row.isReal && (
          <span
            data-element="DepthBar"
            className={`absolute top-0.5 bottom-0.5 right-1 ${depthClass} rounded-bar`}
            style={{ width: `${Math.max(2, cumPct)}%`, zIndex: 0 }}
          />
        )}
        <span className="relative z-[1]">{row.isReal ? formatQty(row.qty) : '—'}</span>
      </td>
      <td
        data-element="RowOrders"
        className="num text-ink-2 px-8 py-1.5 text-right"
      >
        {row.isReal ? '1' : '—'}
      </td>
      <td
        data-element="RowCum"
        className="num text-ink-2 px-8 py-1.5 text-right"
      >
        {row.isReal ? formatQty(row.qty) : '—'}
      </td>
    </tr>
  );
}

function SpreadRow({ bidPx, askPx }: { bidPx: number | null; askPx: number | null }) {
  const hasSpread = bidPx !== null && askPx !== null;
  return (
    <tr
      data-element="SpreadRow"
      className="bg-spread-band border-y"
      style={{ borderColor: 'rgba(212, 162, 76, 0.3)' }}
    >
      <td colSpan={4} className="font-display italic text-ladder text-ink-soft text-center py-3.5">
        spread{' '}
        <span className="num not-italic text-gold mx-2 font-medium">
          {hasSpread ? (askPx - bidPx).toFixed(2) : '—'}
        </span>{' '}
        · mid{' '}
        <span className="num not-italic text-gold mx-2 font-medium">
          {hasSpread ? ((askPx + bidPx) / 2).toFixed(2) : '—'}
        </span>
      </td>
    </tr>
  );
}

export function Ladder() {
  const top = useDashboard((s) => s.displayedTop);
  const state = useDashboard((s) => s.connectionState);
  const ready = state !== 'connecting';

  // Build 8 ask rows (highest first, lowest at the spread) and 8 bid
  // rows (highest at the spread, lowest at the bottom). The row
  // closest to the spread on each side is the real TOB entry; the
  // others are placeholders pending the L8 depth payload.
  const askRows: Row[] = Array.from({ length: ROWS_PER_SIDE }, (_, i) => {
    const isLast = i === ROWS_PER_SIDE - 1;
    return ready && isLast && top !== null && top.askPx !== null
      ? { px: top.askPx, qty: top.askQty, isReal: true }
      : { px: null, qty: 0, isReal: false };
  });
  const bidRows: Row[] = Array.from({ length: ROWS_PER_SIDE }, (_, i) => {
    return ready && i === 0 && top !== null && top.bidPx !== null
      ? { px: top.bidPx, qty: top.bidQty, isReal: true }
      : { px: null, qty: 0, isReal: false };
  });

  return (
    <section data-component="Ladder" className="px-8 py-9">
      <SectionHead title="the book" aux="L1 today · L8 with extended snapshot" />
      <table className="w-full font-mono text-ladder" style={{ borderCollapse: 'collapse' }}>
        <thead data-element="LadderHead">
          <tr>
            <th className="font-body font-medium uppercase text-micro text-ink-faint tracking-aux text-left px-8 py-1.5">
              price
            </th>
            <th className="font-body font-medium uppercase text-micro text-ink-faint tracking-aux text-right px-8 py-1.5">
              size
            </th>
            <th className="font-body font-medium uppercase text-micro text-ink-faint tracking-aux text-right px-8 py-1.5">
              orders
            </th>
            <th className="font-body font-medium uppercase text-micro text-ink-faint tracking-aux text-right px-8 py-1.5">
              cum
            </th>
          </tr>
        </thead>
        <tbody data-element="LadderAsks">
          {askRows.map((r, i) => (
            <LadderRow
              key={`ask-${i}`}
              row={r}
              side="ask"
              cumPct={r.isReal ? 100 : 0}
            />
          ))}
        </tbody>
        <tbody>
          <SpreadRow bidPx={top?.bidPx ?? null} askPx={top?.askPx ?? null} />
        </tbody>
        <tbody data-element="LadderBids">
          {bidRows.map((r, i) => (
            <LadderRow
              key={`bid-${i}`}
              row={r}
              side="bid"
              cumPct={r.isReal ? 100 : 0}
            />
          ))}
        </tbody>
      </table>
    </section>
  );
}
