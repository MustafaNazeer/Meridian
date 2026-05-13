import { formatPrice, formatQty } from '../lib/format';
import { useDashboard } from '../store/dashboard';
import { SectionHead } from './SectionHead';

const ROWS_PER_SIDE = 8;

type Row = {
  px: number | null;
  qty: number;
  cum: number;
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
        <span className="relative z-[1]">
          {row.isReal ? formatQty(row.qty) : '—'}
        </span>
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
        {row.isReal ? formatQty(row.cum) : '—'}
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
        {'·'} mid{' '}
        <span className="num not-italic text-gold mx-2 font-medium">
          {hasSpread ? ((askPx + bidPx) / 2).toFixed(2) : '—'}
        </span>
      </td>
    </tr>
  );
}

function buildAskRows(depth: { px: number; qty: number }[] | undefined): Row[] {
  // Asks in the store come closest-to-spread first. We render them
  // top-down in the table with the deepest level at the top and the
  // level closest to the spread at the bottom (just above the spread
  // row). Cumulative qty is computed from the closest level outward,
  // so index 0 in the store has the smallest cumulative value.
  const cums: number[] = [];
  let cum = 0;
  for (const level of depth ?? []) {
    cum += level.qty;
    cums.push(cum);
  }
  return Array.from({ length: ROWS_PER_SIDE }, (_, displayIdx) => {
    // displayIdx 0 is the top row (deepest ask); displayIdx ROWS_PER_SIDE-1
    // is the row closest to the spread.
    const storeIdx = ROWS_PER_SIDE - 1 - displayIdx;
    const level = depth?.[storeIdx];
    if (!level) return { px: null, qty: 0, cum: 0, isReal: false };
    return {
      px: level.px,
      qty: level.qty,
      cum: cums[storeIdx],
      isReal: true,
    };
  });
}

function buildBidRows(depth: { px: number; qty: number }[] | undefined): Row[] {
  // Bids in the store come closest-to-spread first (index 0 = best bid).
  // We render them top-down with the best bid at the top (just below the
  // spread row) and the deepest level at the bottom.
  const cums: number[] = [];
  let cum = 0;
  for (const level of depth ?? []) {
    cum += level.qty;
    cums.push(cum);
  }
  return Array.from({ length: ROWS_PER_SIDE }, (_, i) => {
    const level = depth?.[i];
    if (!level) return { px: null, qty: 0, cum: 0, isReal: false };
    return {
      px: level.px,
      qty: level.qty,
      cum: cums[i],
      isReal: true,
    };
  });
}

export function Ladder() {
  const depth = useDashboard((s) => s.displayedDepth);
  const top = useDashboard((s) => s.displayedTop);
  const state = useDashboard((s) => s.connectionState);
  const ready = state !== 'connecting';

  const askRows = ready ? buildAskRows(depth?.asks) : buildAskRows(undefined);
  const bidRows = ready ? buildBidRows(depth?.bids) : buildBidRows(undefined);

  const maxCumAsk = askRows.reduce((m, r) => Math.max(m, r.cum), 0) || 1;
  const maxCumBid = bidRows.reduce((m, r) => Math.max(m, r.cum), 0) || 1;

  return (
    <section data-component="Ladder" className="px-8 py-9">
      <SectionHead title="the book" aux="L8" />
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
              cumPct={r.isReal ? (r.cum / maxCumAsk) * 100 : 0}
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
              cumPct={r.isReal ? (r.cum / maxCumBid) * 100 : 0}
            />
          ))}
        </tbody>
      </table>
    </section>
  );
}
