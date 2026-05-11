import { SectionHead } from './SectionHead';

const ROWS = 12;

// The tape is intentionally empty in v1: the wire payload is TOB-only,
// so there is no honest source of trade prints. The canonical
// anatomy (head row + 12 body rows) renders as skeleton rows so the
// table's height matches the canonical layout and the eventual
// follow-up payload slots in without a layout shift.
export function Tape() {
  return (
    <section data-component="Tape">
      <SectionHead
        title="the tape"
        aux="prints land with extended snapshot"
      />
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
          {Array.from({ length: ROWS }, (_, i) => (
            <tr
              key={i}
              data-element="TapeRow"
              className="border-b border-rule-tape"
            >
              <td className="num text-ink-faint px-3 py-1.5">—</td>
              <td className="px-3 py-1.5 text-ink-faint">—</td>
              <td className="num text-ink-faint px-3 py-1.5 text-right">—</td>
              <td className="num text-ink-faint px-3 py-1.5 text-right">—</td>
              <td className="num text-ink-faint px-3 py-1.5 text-right">—</td>
              <td className="px-3 py-1.5 text-ink-faint text-right">—</td>
            </tr>
          ))}
        </tbody>
      </table>
    </section>
  );
}
