// Build-time constants surfaced in the footer. Populated by Vite at
// build time from `VITE_*` env vars; defaults below keep the dashboard
// readable in `pnpm run dev` without env wiring. The deploy pipeline
// is expected to set these via `.env.production` or the Pages dashboard.

export type BuildInfo = {
  version: string;
  compiler: string;
  flags: string;
  websocketUrl: string;
};

function readEnv(key: string, fallback: string): string {
  const env = import.meta.env as Record<string, string | undefined>;
  const v = env[key];
  return v && v.length > 0 ? v : fallback;
}

export function buildInfo(): BuildInfo {
  return {
    version: readEnv('VITE_MERIDIAN_VERSION', '0.1.0-dev'),
    compiler: readEnv('VITE_MERIDIAN_COMPILER', 'clang 19.1.7'),
    flags: readEnv('VITE_MERIDIAN_FLAGS', '-O3 -march=native -DNDEBUG'),
    websocketUrl: readEnv('VITE_MERIDIAN_WS', 'ws://localhost:8080/ws'),
  };
}
