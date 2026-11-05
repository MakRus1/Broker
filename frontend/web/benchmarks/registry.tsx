import { useLayoutEffect, useMemo, useRef, useState } from 'react';
import { createRoot } from 'react-dom/client';
import { RegistryTable, type RegistryColumn } from '../src/features/registry/RegistryTable';
import { restoreColumns } from '../src/features/registry/registryPreferences';
import '../src/app/style.css';

type Row = { id: string; value: string };
function Benchmark() {
  const [scenario, setScenario] = useState({ rows: 100, columns: 10, tabs: 1, run: 0 });
  const [result, setResult] = useState('Выберите сценарий');
  const started = useRef(0);
  const [active, setActive] = useState(0);
  const definitions = useMemo<RegistryColumn<Row>[]>(
    () =>
      Array.from({ length: scenario.columns }, (_, index) => ({
        id: `column-${index}`,
        labelKey: 'registryField.name',
        width: 160,
        render: (item) => `${item.value} / ${index + 1}`,
      })),
    [scenario.columns],
  );
  const items = useMemo(
    () =>
      Array.from({ length: scenario.rows }, (_, index) => ({
        id: String(index),
        value: `Строка ${index + 1}`,
      })),
    [scenario.rows],
  );
  const columns = useMemo(
    () => ({
      columns: restoreColumns(null, definitions),
      definitions,
      setColumns: () => {},
      newColumns: [],
      acknowledgeColumns: () => {},
      guidanceKey: '',
    }),
    [definitions],
  );
  useLayoutEffect(() => {
    if (!started.current) return;
    let second = 0;
    const first = requestAnimationFrame(() => {
      second = requestAnimationFrame(() => {
        setResult(
          JSON.stringify({
            rowsPerTab: scenario.rows,
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
  }, [scenario, active]);
  function run(rows: number, columns: number, tabs: number) {
    started.current = performance.now();
    setResult('Измерение…');
    setActive(0);
    setScenario((old) => ({ rows, columns, tabs, run: old.run + 1 }));
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
        <button onClick={() => run(1000, 20, 1)}>1000 × 20</button>
        <button onClick={() => run(1000, 100, 1)}>1000 × 100</button>
        <button onClick={() => run(100, 20, 10)}>10 вкладок × 100 × 20</button>
        <button
          onClick={() => {
            started.current = performance.now();
            setActive((old) => (old + 1) % scenario.tabs);
          }}
        >
          Следующая вкладка
        </button>
      </div>
      <pre role="status" style={{ whiteSpace: 'pre-wrap' }}>
        {result}
      </pre>
      {Array.from({ length: scenario.tabs }, (_, index) => (
        <div key={`${scenario.run}-${index}`} hidden={active !== index}>
          <RegistryTable
            items={items}
            definitions={definitions}
            columns={columns}
            onOpen={() => {}}
          />
        </div>
      ))}
    </main>
  );
}
createRoot(document.getElementById('root')!).render(<Benchmark />);
