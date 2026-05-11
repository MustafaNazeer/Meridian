import type { Symbol } from '../types';

// Per-symbol static metadata: long name, market, and a stub "30 day
// average volume" used only for the hero "vs 30d avg" sub line. These
// values are not derived from a feed; they are display fixtures so the
// chrome around the live data reads as an instrument card rather than
// an abstract test harness. When real ITCH replay lands, the volume
// stat will be sourced from the engine and the avg line will become
// either real or removed.

export type SymbolMeta = {
  symbol: Symbol;
  longName: string;
  market: string;
  avgVolumeMillions: number;
};

const META: Record<Symbol, SymbolMeta> = {
  AAPL: {
    symbol: 'AAPL',
    longName: 'Apple Inc',
    market: 'NASDAQ Global Select',
    avgVolumeMillions: 58.4,
  },
  SPY: {
    symbol: 'SPY',
    longName: 'SPDR S&P 500 ETF Trust',
    market: 'NYSE Arca',
    avgVolumeMillions: 72.1,
  },
  NVDA: {
    symbol: 'NVDA',
    longName: 'NVIDIA Corporation',
    market: 'NASDAQ Global Select',
    avgVolumeMillions: 184.2,
  },
  TSLA: {
    symbol: 'TSLA',
    longName: 'Tesla, Inc.',
    market: 'NASDAQ Global Select',
    avgVolumeMillions: 96.7,
  },
  GOOG: {
    symbol: 'GOOG',
    longName: 'Alphabet Inc Class C',
    market: 'NASDAQ Global Select',
    avgVolumeMillions: 21.5,
  },
};

export function symbolMeta(s: Symbol): SymbolMeta {
  return META[s];
}
