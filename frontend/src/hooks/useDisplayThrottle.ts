import { useEffect } from 'react';
import { useDashboard } from '../store/dashboard';

// Default throttle interval. 250 ms (4 Hz) is roughly the slowest the
// eye can absorb numeric updates without feeling broken; faster reads
// as flicker, slower reads as stale. The Bloomberg / Refinitiv
// terminal cadence is in the 2 to 5 Hz range and this lands in the
// middle of that band.
const DEFAULT_INTERVAL_MS = 250;

// Throttle the rendered editorial cells (Hero stats, Ladder, Tape) to
// a human-legible cadence while the underlying WebSocket stream keeps
// running at 30 Hz for the perf panel and depth chart inputs. The
// throttle works by mirroring `top` into `displayedTop` on a fixed
// interval; selectors reading `displayedTop` only re-render when the
// store entry changes reference.
export function useDisplayThrottle(intervalMs: number = DEFAULT_INTERVAL_MS): void {
  const flush = useDashboard((s) => s.flushDisplayed);
  useEffect(() => {
    const id = window.setInterval(flush, intervalMs);
    return () => window.clearInterval(id);
  }, [flush, intervalMs]);
}
