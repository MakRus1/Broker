import '../../shared/theme/tokens.css';
import '../src/app/style.css';
import { useCallback, useState } from 'react';
import { createRoot } from 'react-dom/client';
import {
  RegistryTable,
  useRegistryColumns,
  type RegistryColumn,
} from '../src/features/registry/RegistryTable';
import { usePagedRowSelection } from '../src/features/registry/usePagedRowSelection';
import { useRegistryPage } from '../src/features/registry/useRegistryPage';
type Row = { id: string; payload: string };
function Benchmark() {
  const [requests, setRequests] = useState(0);
  const [error, setError] = useState('');
  const [active, setActive] = useState(true);
  const loadPage = useCallback(async (cursor: number | undefined, signal: AbortSignal) => {
    setRequests((n) => n + 1);
    await new Promise<void>((resolve, reject) => {
      const timer = setTimeout(resolve, 20);
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
    return {
      items: Array.from({ length: 50 }, (_, i) => ({
        id: String(start + i),
        payload: 'x'.repeat(10000),
      })),
      next_cursor: start + 50 < 100000 ? start + 50 : undefined,
    };
  }, []);
  const page = useRegistryPage<Row, number>({
    active,
    refreshVersion: 0,
    loadPage,
    onError: (e) => setError(String(e)),
  });
  const selection = usePagedRowSelection(page, 500, active);
  const definitions: RegistryColumn<Row>[] = [
    { id: 'id', labelKey: 'registryField.name', width: 300, render: (row) => `Строка ${row.id}` },
  ];
  const columns = useRegistryColumns('benchmark', 'page-cache', definitions);
  return (
    <main style={{ padding: 24 }}>
      <div
        style={{
          position: 'sticky',
          top: 0,
          zIndex: 10,
          background: 'var(--surface)',
          padding: 12,
        }}
      >
        <output>
          rows={page.count}; resident={page.items.length}; requests={requests}; error={error};
          selected={selection.selected.size}; resolved={selection.items.length}; resolving=
          {String(selection.resolving)}; selectionError={selection.error}
        </output>
        <button
          onClick={async () => {
            for (let i = 0; i < 30; i++) await page.loadMore();
          }}
        >
          Load 30 pages
        </button>
        <button onClick={() => selection.setSelected(new Set([page.ids[0], page.ids.at(-1)!]))}>
          Select first and last
        </button>
        <button onClick={selection.clear}>Clear selection</button>
        <button onClick={() => setActive(!active)}>Switch tab</button>
        <button onClick={() => page.refresh()}>Refresh</button>
        <button onClick={() => window.scrollTo(0, document.body.scrollHeight)}>Bottom</button>
        <button onClick={() => window.scrollTo(0, 0)}>Top</button>
      </div>
      <div hidden={!active}>
        <RegistryTable
          items={page.items}
          pageSource={page.source}
          selectionColumn={{
            header: null,
            render: (row) => (
              <input
                type="checkbox"
                checked={selection.selected.has(row.id)}
                onChange={(e) => selection.toggle(row.id, e.target.checked)}
              />
            ),
          }}
          columns={columns}
          definitions={definitions}
          onOpen={() => {}}
        />
      </div>
    </main>
  );
}
createRoot(document.getElementById('root')!).render(<Benchmark />);
