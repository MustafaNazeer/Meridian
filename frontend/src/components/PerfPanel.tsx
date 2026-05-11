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

function PerfHistogramSkeleton() {
  // 27 vertical placeholder bars matching the canonical anatomy. The
  // engine latency histogram is not on the wire today; the bars render
  // at uniform low height so the panel keeps its shape without faking
  // a distribution.
  return (
    <div
      data-element="PerfHistogram"
      className="flex items-end gap-px"
      style={{ height: '48px' }}
    >
      {Array.from({ length: 27 }, (_, i) => (
        <div
          key={i}
          data-element="HistogramBar"
          className="flex-1 bg-rule rounded-bar"
          style={{ height: '6px' }}
        />
      ))}
    </div>
  );
}

function PerfLatency() {
  return (
    <div data-element="PerfLatency" className="mb-8">
      <PerfLabel text="engine latency" />
      <PerfHistogramSkeleton />
      <div className="font-display italic text-meta text-ink-soft mt-2 mb-3.5">
        distribution lands with extended snapshot
      </div>
      <div className="grid grid-cols-perf gap-x-5.5 gap-y-3.5">
        <PerfCell label="p50" value="—" />
        <PerfCell label="p99" value="—" />
        <PerfCell label="p99.9" value="—" />
        <PerfCell label="max" value="—" />
      </div>
    </div>
  );
}

function PerfBookState() {
  const top = useDashboard((s) => s.top);
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
          tape input · <strong className="text-ink font-medium">extended snapshot follow-up</strong>
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
