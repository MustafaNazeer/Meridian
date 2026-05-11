import { useEffect } from 'react';
import { buildInfo } from '../lib/build';
import { useDashboard } from '../store/dashboard';
import { decodeTopOfBook } from '../types';
import { MeridianWsClient } from '../ws/client';

// Mounts a singleton WebSocket client on initial render and bridges
// its callbacks into the Zustand store. The selected symbol is read
// from the store on mount; this component does not re-mount the
// client on symbol change today (the server is a single-symbol demo,
// so symbol switching is a frontend-only re-target until the extended
// snapshot payload lands).
export function useMeridianStream(): void {
  const setConnectionState = useDashboard((s) => s.setConnectionState);
  const applySnapshot = useDashboard((s) => s.applySnapshot);
  const applyDelta = useDashboard((s) => s.applyDelta);
  const notePerfTick = useDashboard((s) => s.notePerfTick);
  const setReconnect = useDashboard((s) => s.setReconnect);

  useEffect(() => {
    const url = buildInfo().websocketUrl;
    const client = new MeridianWsClient(url, {
      onState: setConnectionState,
      onSnapshot: (msg, bytes) => applySnapshot(decodeTopOfBook(msg), bytes),
      onDelta: (msg, bytes) => applyDelta(decodeTopOfBook(msg), bytes),
      onPerfTick: (deltas, bytes) => notePerfTick({ deltas, bytes }),
      onReconnect: (attempt, inMs) => setReconnect(attempt, inMs),
    });
    client.start();
    return () => client.stop();
  }, [setConnectionState, applySnapshot, applyDelta, notePerfTick, setReconnect]);
}
