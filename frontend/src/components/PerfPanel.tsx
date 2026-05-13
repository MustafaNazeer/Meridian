import { formatBytes, formatThousands } from '../lib/format';
import { useDashboard } from '../store/dashboard';
import { SectionHead } from './SectionHead';

function PerfLabel({ text }: { text: string }) {
  return (
    <div
      data-element="PerfLabel"
      className="font-display italic text-trades text-gold mb-2.5"
    >
      {text}
    </div>
  );
}

function PerfCell({ label, value }: { label: string; value: string }) {
  return (
    <div data-element="PerfCell">
      <div className="font-body uppercase text-micro text-ink-soft tracking-perf-label mb-0.75">
        {label}
      </div>
      <div className="num font-light text-perf-cell text-ink">{value}</div>
    </div>
  );
}

function PerfThroughput() {
  const deltas = useDashboard((s) => s.wire.deltasPerSec);
  const bytes = useDashboard((s) => s.wire.bytesPerSec);
  return (
    <div data-element="PerfThroughput" className="mb-8">
      <PerfLabel text="wire throughput (client view)" />
      <div data-element="PerfHeadline" className="flex items-baseline leading-1">
        <span className="num font-light text-display-sm text-ink tracking-tight-display">
          {deltas.toFixed(0)}
        </span>
        <span className="font-display italic text-trades text-ink-soft ml-2">
          deltas / sec
        </span>
      </div>
      <div className="font-display italic text-meta text-ink-soft mt-2">
        {formatBytes(bytes)} / sec · sampler caps engine at 30 Hz
      </div>
    </div>
  );
}

function PerfHistogram({ hist }: { hist: number[] }) {
  const maxCount = hist.reduce((m, c) => Math.max(m, c), 0) || 1;
  return (
    <div
      data-element="PerfHistogram"
      className="flex items-end gap-px"
      style={{ height: '48px' }}
    >
      {hist.map((c, i) => {
        const pct = (c / maxCount) * 100;
        const heightPx = Math.max(2, (pct / 100) * 48);
        return (
          <div
            key={i}
            data-element="HistogramBar"
            className="flex-1 bg-gold rounded-bar"
            style={{ height: `${heightPx}px`, opacity: c > 0 ? 0.85 : 0.15 }}
          />
        );
      })}
    </div>
  );
}

function formatNs(ns: number): string {
  if (ns === 0) return '—';
  if (ns >= 1_000_000) return `${(ns / 1_000_000).toFixed(2)} ms`;
  if (ns >= 1_000) return `${(ns / 1_000).toFixed(2)} µs`;
  return `${ns} ns`;
}

function PerfLatency() {
  const latency = useDashboard((s) => s.displayedLatency);
  const hist = latency?.hist ?? Array(33).fill(0);
  const samples = latency?.samples ?? 0;
  const p50 = latency?.p50Ns ?? 0;
  const p99 = latency?.p99Ns ?? 0;
  const p999 = samples >= 1000 ? (latency?.p999Ns ?? 0) : 0;
  const max = latency?.maxNs ?? 0;
  return (
    <div data-element="PerfLatency" className="mb-8">
      <PerfLabel text="engine latency" />
      <PerfHistogram hist={hist} />
      <div className="font-display italic text-meta text-ink-soft mt-2 mb-3.5">
        {samples > 0 ? `${samples.toLocaleString()} samples since start` : 'no samples yet'}
      </div>
      <div className="grid grid-cols-perf gap-x-5.5 gap-y-3.5">
        <PerfCell label="p50"   value={formatNs(p50)} />
        <PerfCell label="p99"   value={formatNs(p99)} />
        <PerfCell label="p99.9" value={formatNs(p999)} />
        <PerfCell label="max"   value={formatNs(max)} />
      </div>
    </div>
  );
}

function PerfBookState() {
  const top = useDashboard((s) => s.displayedTop);
  const ticks = useDashboard((s) => s.ticksSinceConnect);
  return (
    <div data-element="PerfBookState" className="mb-8">
      <PerfLabel text="book state (client view)" />
      <div className="grid grid-cols-perf gap-x-5.5 gap-y-3.5">
        <PerfCell
          label="best bid qty"
          value={top?.bidQty !== undefined ? formatThousands(top.bidQty) : '—'}
        />
        <PerfCell
          label="best ask qty"
          value={top?.askQty !== undefined ? formatThousands(top.askQty) : '—'}
        />
        <PerfCell label="ticks seen" value={formatThousands(ticks)} />
        <PerfCell
          label="event ts"
          value={top?.ts !== undefined ? formatThousands(top.ts) : '—'}
        />
      </div>
    </div>
  );
}

function PerfReplay() {
  return (
    <div data-element="PerfReplay">
      <PerfLabel text="replay" />
      <div
        data-element="ReplayCopy"
        className="font-mono text-meta text-ink-soft leading-replay"
      >
        <div>
          source · <strong className="text-ink font-medium">synthetic generator</strong>
        </div>
        <div>
          mix · <strong className="text-ink font-medium">limit / market / cancel</strong>
        </div>
        <div>
          tape input {'·'} <strong className="text-ink font-medium">live trade ring</strong>
        </div>
      </div>
      <div
        data-element="ProgressBar"
        className="bg-rule mt-3.5"
        style={{ height: '2px' }}
      >
        <div
          data-element="ProgressFill"
          className="h-full bg-replay-progress shadow-glow-gold"
          style={{ width: '0%' }}
        />
      </div>
    </div>
  );
}

export function PerfPanel() {
  return (
    <section data-component="PerfPanel" className="px-8 py-9">
      <SectionHead title="performance" aux="single thread" />
      <PerfThroughput />
      <PerfLatency />
      <PerfBookState />
      <PerfReplay />
    </section>
  );
}
