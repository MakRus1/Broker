import '../../shared/theme/tokens.css';
import '../src/features/finance/services/bulkServices.css';
import { useEffect, useLayoutEffect, useMemo, useRef, useState } from 'react';
import { createRoot } from 'react-dom/client';
import { RegistryTable, type RegistryColumn } from '../src/features/registry/RegistryTable';
import { useRowSelection } from '../src/features/registry/useRowSelection';
import {
  restoreColumns,
  type ColumnPreference,
} from '../src/features/registry/registryPreferences';
import '../src/app/style.css';

type Row = { id: string; value: string; cells?: string[] };
function Benchmark() {
  const [scenario, setScenario] = useState({ rows: 100, columns: 10, tabs: 1, run: 0 });
  const [result, setResult] = useState('Выберите сценарий');
  const started = useRef(0);
  const network = useRef<Record<string, number | string>>({});
  const longTasks = useRef<{ startTime: number; duration: number }[]>([]);
  const [remoteItems, setRemoteItems] = useState<Row[] | null>(null);
  useEffect(() => {
    if (!PerformanceObserver.supportedEntryTypes.includes('longtask')) return;
    const observer = new PerformanceObserver((list) => {
      longTasks.current.push(
        ...list.getEntries().map(({ startTime, duration }) => ({ startTime, duration })),
      );
    });
    observer.observe({ type: 'longtask', buffered: true });
    return () => observer.disconnect();
  }, []);
  const [active, setActive] = useState(0);
  const [virtualize, setVirtualize] = useState(true);
  const definitions = useMemo<RegistryColumn<Row>[]>(
    () =>
      Array.from({ length: scenario.columns }, (_, index) => ({
        id: `column-${index}`,
        labelKey: 'registryField.name',
        width: 160,
        render: (item) =>
          index === 0 && Number(item.id) % 7 === 0 ? (
            <span>
              {item.value}
              <br />
              Длинный маршрут
              <br />
              Второй участок
            </span>
          ) : (
            (item.cells?.[index] ?? `${item.value} / ${index + 1}`)
          ),
      })),
    [scenario.columns],
  );
  const items = useMemo(
    () =>
      remoteItems ??
      Array.from({ length: scenario.rows }, (_, index) => ({
        id: String(index),
        value: `Строка ${index + 1}`,
      })),
    [scenario.rows, remoteItems],
  );
  const selection = useRowSelection(items.map((item) => item.id));
  const [savedColumns, setSavedColumns] = useState<ColumnPreference[] | null>(null);
  const columns = useMemo(
    () => ({
      columns: restoreColumns(savedColumns, definitions),
      definitions,
      setColumns: (next: ColumnPreference[] | ((old: ColumnPreference[]) => ColumnPreference[])) =>
        setSavedColumns((old) =>
          typeof next === 'function' ? next(restoreColumns(old, definitions)) : next,
        ),
      newColumns: [],
      acknowledgeColumns: () => {},
      guidanceKey: '',
    }),
    [definitions, savedColumns],
  );
  useLayoutEffect(() => {
    if (!started.current) return;
    let second = 0;
    const first = requestAnimationFrame(() => {
      second = requestAnimationFrame(() => {
        setResult(
          JSON.stringify({
            ...network.current,
            rowsPerTab: items.length,
            longTaskObserverSupported: PerformanceObserver.supportedEntryTypes.includes('longtask'),
            longTasks: longTasks.current.filter((task) => task.startTime >= started.current).length,
            longestTaskMs: Math.max(
              0,
              ...longTasks.current
                .filter((task) => task.startTime >= started.current)
                .map((task) => task.duration),
            ),
            columns: scenario.columns,
            tabs: scenario.tabs,
            elapsedToTwoFramesMs: Math.round((performance.now() - started.current) * 10) / 10,
            domElements: document.querySelectorAll('*').length,
            renderedRows: document.querySelectorAll('tbody tr').length,
          }),
        );
      });
    });
    return () => {
      cancelAnimationFrame(first);
      cancelAnimationFrame(second);
    };
  }, [scenario, active, virtualize]);
  function run(rows: number, columns: number, tabs: number) {
    started.current = performance.now();
    network.current = {};
    setRemoteItems(null);
    setResult('Измерение…');
    setActive(0);
    setScenario((old) => ({ rows, columns, tabs, run: old.run + 1 }));
  }
  async function loadWide() {
    const start = performance.now();
    setResult('Загрузка полного набора 100000 × 100…');
    const response = await fetch('/wide.json');
    const headersAt = performance.now();
    const text = await response.text();
    const receivedAt = performance.now();
    if (!response.ok) {
      setResult(`HTTP ${response.status}`);
      return;
    }
    const payload = JSON.parse(text) as { items: Row[] };
    const parsedAt = performance.now();
    network.current = {
      ttfbMs: headersAt - start,
      receiveMs: receivedAt - headersAt,
      parseMs: parsedAt - receivedAt,
      bytes: new TextEncoder().encode(text).length,
    };
    started.current = start;
    setRemoteItems(payload.items);
    setActive(0);
    setScenario((old) => ({ rows: payload.items.length, columns: 100, tabs: 1, run: old.run + 1 }));
  }
  async function loadPage() {
    const start = performance.now();
    setResult('Запрос к изолированному BFF…');
    const response = await fetch('/api/v1/transportations/list', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ limit: 100, sort: 'container_number' }),
    });
    const headersAt = performance.now();
    const text = await response.text();
    const receivedAt = performance.now();
    if (!response.ok) {
      setResult(`HTTP ${response.status}: ${text}`);
      return;
    }
    const payload = JSON.parse(text);
    const parsedAt = performance.now();
    network.current = {
      ttfbMs: headersAt - start,
      receiveMs: receivedAt - headersAt,
      parseMs: parsedAt - receivedAt,
      bytes: new TextEncoder().encode(text).length,
      serverTiming: response.headers.get('Server-Timing') ?? '',
    };
    started.current = start;
    setRemoteItems(
      payload.items.map((item: { id: string; container_number?: string }) => ({
        id: item.id,
        value: item.container_number ?? 'Без номера',
      })),
    );
    setActive(0);
    setScenario((old) => ({ rows: payload.items.length, columns: 100, tabs: 1, run: old.run + 1 }));
  }
  return (
    <main style={{ padding: 24 }}>
      <h1>Проверка реестра на синтетических данных</h1>
      <p>
        Production-сборка реального RegistryTable. Время от действия до двух кадров; это не INP.
        Скрытые таблицы остаются смонтированными.
      </p>
      <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap' }}>
        <button onClick={() => run(100, 10, 1)}>100 × 10</button>
        <button onClick={() => run(100, 100, 1)}>100 × 100</button>
        <button onClick={() => run(1000, 20, 1)}>1000 × 20</button>
        <button onClick={() => run(1000, 100, 1)}>1000 × 100</button>
        <button onClick={() => run(100000, 100, 1)}>100000 × 100</button>
        <button onClick={loadWide}>100000 × 100: полные данные по HTTP</button>
        <button onClick={() => run(100000, 100, 10)}>10 вкладок × 100000 × 100</button>
        <button onClick={loadPage}>Полный путь: API → 100 строк × 100 столбцов</button>
        <button onClick={() => run(100, 20, 10)}>10 вкладок × 100 × 20</button>
        <button onClick={() => run(1000, 20, 10)}>10 вкладок × 1000 × 20</button>
        <button
          onClick={() => {
            started.current = performance.now();
            setActive((old) => (old + 1) % scenario.tabs);
          }}
        >
          Следующая вкладка
        </button>
      </div>
      <label>
        <input
          type="checkbox"
          checked={virtualize}
          onChange={(event) => {
            started.current = performance.now();
            setVirtualize(event.currentTarget.checked);
          }}
        />
        Виртуализация
      </label>
      <p>Выбрано: {selection.selected.size}</p>
      <pre role="status" style={{ whiteSpace: 'pre-wrap' }}>
        {result}
      </pre>
      {Array.from({ length: scenario.tabs }, (_, index) => (
        <div key={`${scenario.run}-${index}`} hidden={active !== index}>
          <RegistryTable
            virtualize={virtualize}
            items={items}
            definitions={definitions}
            columns={columns}
            onOpen={() => {}}
            selectionColumn={{
              header: <span>Выбор</span>,
              render: (item) => (
                <input
                  type="checkbox"
                  aria-label={`Выбрать ${item.id}`}
                  checked={selection.selected.has(item.id)}
                  onClick={(event) => event.stopPropagation()}
                  onChange={(event) =>
                    selection.toggle(
                      item.id,
                      event.currentTarget.checked,
                      (event.nativeEvent as MouseEvent).shiftKey,
                    )
                  }
                />
              ),
            }}
          />
        </div>
      ))}
    </main>
  );
}
createRoot(document.getElementById('root')!).render(<Benchmark />);
