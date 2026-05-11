import { buildInfo } from '../lib/build';

export function Footer() {
  const info = buildInfo();
  return (
    <footer
      data-component="Footer"
      className="flex items-center justify-between border-t border-rule px-12 py-3.5 bg-bg-deep"
    >
      <div
        data-element="FooterRuntime"
        className="font-body text-meta text-ink-soft tracking-stat"
      >
        <strong className="text-ink font-medium">matching</strong> single-threaded ·{' '}
        <strong className="text-ink font-medium">sampler</strong> 30 Hz seqlock read ·{' '}
        <strong className="text-ink font-medium">ws</strong> hand-rolled poll() loop · zero blocking on the hot path
      </div>
      <div
        data-element="FooterBuild"
        className="font-body text-meta text-ink-faint tracking-stat"
      >
        meridian v{info.version} · {info.compiler} · {info.flags}
      </div>
    </footer>
  );
}
