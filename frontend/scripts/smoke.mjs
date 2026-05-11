// Tiny smoke harness: spins up the same MeridianWsClient the React app
// mounts, talks to a running meridian-server at ws://localhost:18080/ws,
// and prints state transitions, decoded snapshots, and perf ticks.
//
//   node scripts/smoke.mjs
//
// Exits 0 after observing a live state and 3+ decoded messages within
// the deadline; exits 1 otherwise.

import { MeridianWsClient } from '../src/ws/client.ts';

const states = [];
const decoded = [];
let perfTicks = 0;

const client = new MeridianWsClient(
  process.env.MERIDIAN_WS ?? 'ws://localhost:18080/ws',
  {
    onState: (s) => {
      states.push(s);
      console.log(`state -> ${s}`);
    },
    onSnapshot: (msg, bytes) => {
      decoded.push({ kind: 'snapshot', bytes, msg });
      if (decoded.length <= 3) console.log('snapshot', bytes, 'bytes', msg);
    },
    onDelta: (msg, bytes) => {
      decoded.push({ kind: 'delta', bytes, msg });
      if (decoded.length <= 3) console.log('delta', bytes, 'bytes', msg);
    },
    onPerfTick: (d, b) => {
      perfTicks += 1;
      console.log(`perf #${perfTicks}: ${d} msgs, ${b} bytes / 1s`);
    },
    onReconnect: (attempt, inMs) => {
      console.log(`reconnect attempt ${attempt} in ${inMs} ms`);
    },
  },
);

client.start();

const deadline = Date.now() + 5_000;
const id = setInterval(() => {
  if (decoded.length >= 3 && states.includes('live')) {
    console.log('OK', decoded.length, 'messages', perfTicks, 'perf ticks');
    clearInterval(id);
    client.stop();
    process.exit(0);
  }
  if (Date.now() > deadline) {
    console.error('FAIL', { states, count: decoded.length, perfTicks });
    clearInterval(id);
    client.stop();
    process.exit(1);
  }
}, 200);
