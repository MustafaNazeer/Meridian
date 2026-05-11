import { useDashboard } from '../store/dashboard';

export function ConnectionBanner() {
  const state = useDashboard((s) => s.connectionState);
  const inMs = useDashboard((s) => s.reconnectInMs);
  if (state !== 'disconnected') return null;
  const secs = Math.max(0, Math.ceil(inMs / 1000));
  return (
    <div
      data-component="ConnectionBanner"
      className="h-8 w-full flex items-center px-12 bg-ask-soft text-ask font-body text-meta tracking-stat"
    >
      <span data-element="BannerDot" className="inline-block w-1.5 h-1.5 rounded-full bg-ask mr-2.5" />
      socket disconnected · reconnecting in {secs}s
    </div>
  );
}
