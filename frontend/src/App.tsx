import { ConnectionBanner } from './components/ConnectionBanner';
import { DepthChart } from './components/DepthChart';
import { Footer } from './components/Footer';
import { Header } from './components/Header';
import { Hero } from './components/Hero';
import { Ladder } from './components/Ladder';
import { PerfPanel } from './components/PerfPanel';
import { Tape } from './components/Tape';
import { useMeridianStream } from './hooks/useMeridianStream';
import { useDashboard } from './store/dashboard';

function App() {
  useMeridianStream();
  const state = useDashboard((s) => s.connectionState);
  const fadedNumerics = state === 'stalled' || state === 'disconnected';
  return (
    <div
      data-component="App"
      className="bg-body-radial text-ink min-h-screen flex flex-col"
    >
      <Header />
      <ConnectionBanner />
      <Hero />
      <main
        data-component="Main"
        className={
          'flex-1 grid grid-cols-main gap-px bg-rule ' +
          (fadedNumerics ? 'opacity-90' : '')
        }
      >
        <div className="bg-bg">
          <Ladder />
        </div>
        <div className="bg-bg px-8 py-9">
          <DepthChart />
          <Tape />
        </div>
        <div className="bg-bg">
          <PerfPanel />
        </div>
      </main>
      <Footer />
    </div>
  );
}

export default App;
