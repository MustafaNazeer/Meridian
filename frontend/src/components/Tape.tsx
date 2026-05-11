import { formatPrice, formatQty, formatThousands } from '../lib/format';
import { useDashboard } from '../store/dashboard';
import { SectionHead } from './SectionHead';

const ROWS = 12;

export function Tape() {
  // The store's `trades` is oldest-first up to 16 entries. Render
  // newest-first in up to 12 rows. Empty slots render as placeholder
  // rows so the table height stays stable.
  const trades = useDashboard((s) => s.trades);
  const newestFirst = [...trades].reverse().slice(0, ROWS);
  const blanks = Math.max(0, ROWS - newestFirst.length);

  return (
    <section data-component="Tape">
      <SectionHead title="the tape" aux="last 12 prints, engine time" />
      <table className="w-full font-mono text-trades" style={{ borderCollapse: 'collapse' }}>
        <thead data-element="TapeHead">
          <tr>
            <th className="font-body font-medium uppercase text-micro text-ink-faint tracking-aux text-left pt-1.5 pb-2 border-b border-rule">
              time
            </th>
            <th className="font-body font-medium uppercase text-micro text-ink-faint tracking-aux text-left pt-1.5 pb-2 border-b border-rule">
              side
            </th>
            <th className="font-body font-medium uppercase text-micro text-ink-faint tracking-aux text-right pt-1.5 pb-2 border-b border-rule">
              price
            </th>
            <th className="font-body font-medium uppercase text-micro text-ink-faint tracking-aux text-right pt-1.5 pb-2 border-b border-rule">
              size
            </th>
            <th className="font-body font-medium uppercase text-micro text-ink-faint tracking-aux text-right pt-1.5 pb-2 border-b border-rule">
              notional
            </th>
            <th className="font-body font-medium uppercase text-micro text-ink-faint tracking-aux text-right pt-1.5 pb-2 border-b border-rule">
              aggressor
            </th>
          </tr>
        </thead>
        <tbody data-element="TapeBody">
          {newestFirst.map((t, i) => {
            const sideClass = t.aggressor === 'B' ? 'text-bid' : 'text-ask';
            const aggressorLabel = t.aggressor === 'B' ? 'buy' : 'sell';
            const notional = (t.px * t.qty) / 100;
            return (
              <tr
                key={`trade-${t.ts}-${i}`}
                data-element="TapeRow"
                className="border-b border-rule-tape"
              >
                <td className="num text-ink-2 px-3 py-1.5">{formatThousands(t.ts)}</td>
                <td className={`px-3 py-1.5 ${sideClass}`}>{aggressorLabel}</td>
                <td className={`num px-3 py-1.5 text-right ${sideClass}`}>{formatPrice(t.px)}</td>
                <td className="num text-ink-2 px-3 py-1.5 text-right">{formatQty(t.qty)}</td>
                <td className="num text-ink-2 px-3 py-1.5 text-right">{formatPrice(notional)}</td>
                <td className={`px-3 py-1.5 text-right ${sideClass}`}>{aggressorLabel}</td>
              </tr>
            );
          })}
          {Array.from({ length: blanks }, (_, i) => (
            <tr
              key={`blank-${i}`}
              data-element="TapeRow"
              className="border-b border-rule-tape"
            >
              <td className="num text-ink-faint px-3 py-1.5">{'—'}</td>
              <td className="px-3 py-1.5 text-ink-faint">{'—'}</td>
              <td className="num text-ink-faint px-3 py-1.5 text-right">{'—'}</td>
              <td className="num text-ink-faint px-3 py-1.5 text-right">{'—'}</td>
              <td className="num text-ink-faint px-3 py-1.5 text-right">{'—'}</td>
              <td className="px-3 py-1.5 text-ink-faint text-right">{'—'}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </section>
  );
}
