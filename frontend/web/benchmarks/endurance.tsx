import '../../shared/theme/tokens.css';
import '../src/app/style.css';
import { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from 'react';
import { createRoot } from 'react-dom/client';
import {
  RegistryTable,
  useRegistryColumns,
  type RegistryColumn,
} from '../src/features/registry/RegistryTable';
import { useRegistryPage } from '../src/features/registry/useRegistryPage';
type Row = { id: string; cells: string[] };
type Stats = { rows: number; resident: number; requests: number };
type Control = { loadMore: () => Promise<void>; refresh: () => Promise<void> };
const frames = () =>
  new Promise<void>((resolve) =>
    requestAnimationFrame(() => requestAnimationFrame(() => resolve())),
  );
function Registry({
  index,
  active,
  report,
  controls,
}: {
  index: number;
  active: boolean;
  report: (index: number, stats: Stats) => void;
  controls: React.RefObject<Control[]>;
}) {
  const [requests, setRequests] = useState(0);
  const loadPage = useCallback(
    async (cursor: number | undefined, signal: AbortSignal) => {
      setRequests((n) => n + 1);
      await new Promise<void>((resolve, reject) => {
        const timer = setTimeout(resolve, 10);
        signal.addEventListener(
          'abort',
          () => {
            clearTimeout(timer);
            reject(new Error('aborted'));
          },
          { once: true },
        );
      });
      const start = cursor ?? 0;
      // JSON round-trip deliberately materializes 100 fields, rather than sharing one payload string.
      const items = JSON.parse(
        JSON.stringify(
          Array.from({ length: 50 }, (_, i) => ({
            id: String(start + i),
            cells: Array.from(
              { length: 100 },
              (_, col) => `Registry ${index} / row ${start + i} / column ${col} / value`,
            ),
          })),
        ),
      ) as Row[];
      return { items, next_cursor: start + 50 < 100000 ? start + 50 : undefined };
    },
    [index],
  );
  const [error, setError] = useState('');
  const page = useRegistryPage({
    active,
    refreshVersion: 0,
    loadPage,
    onError: (e) => setError(String(e)),
  });
  useLayoutEffect(() => {
    controls.current[index] = { loadMore: page.loadMore, refresh: page.refresh };
  });
  useEffect(
    () => report(index, { rows: page.count, resident: page.items.length, requests }),
    [page.count, page.items.length, requests, index, report],
  );
  const definitions = useMemo<RegistryColumn<Row>[]>(
    () =>
      Array.from({ length: 100 }, (_, col) => ({
        id: `col-${col}`,
        labelKey: 'registryField.name',
        width: 160,
        render: (row) => row.cells[col],
      })),
    [],
  );
  const columns = useRegistryColumns('benchmark', `endurance-${index}`, definitions);
  return (
    <section hidden={!active}>
      {error && <p role="alert">{error}</p>}
      <RegistryTable
        items={page.items}
        pageSource={page.source}
        definitions={definitions}
        columns={columns}
        onOpen={() => {}}
      />
    </section>
  );
}
function Benchmark() {
  const [active, setActive] = useState(0);
  const [stats, setStats] = useState<Record<number, Stats>>({});
  const controls = useRef<Control[]>([]);
  const latestStats = useRef<Record<number, Stats>>({});
  const [running, setRunning] = useState(false);
  const [result, setResult] = useState('Not started');
  const [progress, setProgress] = useState('');
  const tasks = useRef<number[]>([]);
  useEffect(() => {
    if (!PerformanceObserver.supportedEntryTypes.includes('longtask')) return;
    const observer = new PerformanceObserver((list) =>
      tasks.current.push(...list.getEntries().map((e) => e.duration)),
    );
    observer.observe({ type: 'longtask' });
    return () => observer.disconnect();
  }, []);
  const report = useCallback((index: number, value: Stats) => {
    latestStats.current[index] = value;
    setStats((old) => ({ ...old, [index]: value }));
  }, []);
  async function run() {
    setRunning(true);
    tasks.current = [];
    const started = performance.now();
    const switches: number[] = [];
    try {
      // Two rounds preserve all four mounted caches; each table reaches 6,050 rows.
      for (let round = 0; round < 2; round++)
        for (let tab = 0; tab < 4; tab++) {
          setActive(tab);
          await frames();
          // Initial request is also asynchronous: refresh awaits a completed first page once per tab.
          if (round === 0) await controls.current[tab].refresh();
          for (let page = 0; page < 60; page++) {
            await controls.current[tab].loadMore();
            window.scrollTo(0, document.body.scrollHeight);
            await frames();
            setProgress(`round=${round + 1}; tab=${tab + 1}; page=${page + 1}/60`);
          }
          window.scrollTo(0, 0);
          await frames();
        }
      const requestsBeforeSwitch = Object.values(latestStats.current).reduce(
        (n, s) => n + s.requests,
        0,
      );
      for (let repeat = 0; repeat < 3; repeat++)
        for (let tab = 0; tab < 4; tab++) {
          const start = performance.now();
          setActive(tab);
          await frames();
          switches.push(performance.now() - start);
        }
      switches.sort((a, b) => a - b);
      setResult(
        JSON.stringify({
          switchRequestDelta:
            Object.values(latestStats.current).reduce((n, s) => n + s.requests, 0) -
            requestsBeforeSwitch,
          elapsedMs: Math.round(performance.now() - started),
          switchMedianMs: switches[Math.floor(switches.length / 2)],
          switchMaxMs: Math.max(...switches),
          longTaskSupported: PerformanceObserver.supportedEntryTypes.includes('longtask'),
          longTasks: tasks.current.length,
          longestTaskMs: Math.max(0, ...tasks.current),
          dom: document.querySelectorAll('*').length,
          cells: document.querySelectorAll('tbody td').length,
        }),
      );
    } finally {
      setRunning(false);
    }
  }
  return (
    <main style={{ padding: 20 }}>
      <header
        style={{ position: 'sticky', top: 0, zIndex: 5, background: 'var(--surface)', padding: 12 }}
      >
        <h1>4 registries × 100 columns</h1>
        <button disabled={running} onClick={() => void run()}>
          Run endurance
        </button>
        {[0, 1, 2, 3].map((index) => (
          <button disabled={running} key={index} onClick={() => setActive(index)}>
            Registry {index + 1}
          </button>
        ))}
        <p>{progress}</p>
        <output>{result}</output>
        <pre>{JSON.stringify(stats)}</pre>
      </header>
      {[0, 1, 2, 3].map((index) => (
        <Registry
          key={index}
          index={index}
          active={active === index}
          report={report}
          controls={controls}
        />
      ))}
    </main>
  );
}
createRoot(document.getElementById('root')!).render(<Benchmark />);
