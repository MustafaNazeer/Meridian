// Display formatting helpers. Numbers from the wire arrive as integer
// engine ticks where one tick equals one cent (so $100.00 corresponds
// to 10000 ticks). The helpers below divide by 100 to display as
// currency with two decimal places.

const CENTS_PER_DOLLAR = 100;

export function formatPrice(px: number | null): string {
  if (px === null) return '—';
  return (px / CENTS_PER_DOLLAR).toFixed(2);
}

export function formatQty(qty: number): string {
  return qty.toLocaleString('en-US');
}

export function formatSpread(bidPx: number | null, askPx: number | null): {
  abs: string;
  bps: string;
  band: 'tight' | 'normal' | 'wide';
} {
  if (bidPx === null || askPx === null) {
    return { abs: '—', bps: '—', band: 'normal' };
  }
  const spreadCents = askPx - bidPx;
  const mid = (askPx + bidPx) / 2;
  const bps = mid > 0 ? (spreadCents / mid) * 10_000 : 0;
  const band = bps < 4 ? 'tight' : bps > 12 ? 'wide' : 'normal';
  return {
    abs: (spreadCents / CENTS_PER_DOLLAR).toFixed(2),
    bps: bps.toFixed(1),
    band,
  };
}

// "HH:MM:SS.mmm". The engine timestamp is a monotonically increasing
// integer (event count), not a wall clock; we render the host clock so
// the header line reads as a real time while the engine timestamp is
// surfaced separately in the hero meta as "tick {N}".
export function formatWallClock(d: Date): string {
  const hh = String(d.getHours()).padStart(2, '0');
  const mm = String(d.getMinutes()).padStart(2, '0');
  const ss = String(d.getSeconds()).padStart(2, '0');
  const ms = String(d.getMilliseconds()).padStart(3, '0');
  return `${hh}:${mm}:${ss}.${ms}`;
}

export function formatIsoDate(d: Date): string {
  const y = d.getFullYear();
  const m = String(d.getMonth() + 1).padStart(2, '0');
  const day = String(d.getDate()).padStart(2, '0');
  return `${y} · ${m} · ${day}`;
}

export function formatThousands(n: number): string {
  return n.toLocaleString('en-US');
}

export function formatBytes(n: number): string {
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / (1024 * 1024)).toFixed(2)} MB`;
}
