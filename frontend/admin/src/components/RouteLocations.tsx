import { useState } from 'react';
import { request, type Session } from '../api';
import { useRegistry, type RegistryQuery } from '../hooks/useRegistry';
import { RegistryFilters, RegistryMore, SortHeading } from './Registry';

type RouteLocation = {
  id: string;
  kind: 'sea_port' | 'airport';
  name: string;
  country_code: string;
  code?: string;
};
const emptyLocation: RouteLocation = { id: '', kind: 'sea_port', name: '', country_code: '' };

export function RouteLocations({ session }: { session: Session }) {
  const [kind, setKind] = useState<'sea_port' | 'airport'>('sea_port');
  const [filters, setFilters] = useState<RegistryQuery>({ sort: 'name', descending: false });
  const [editor, setEditor] = useState<RouteLocation>();
  const [editing, setEditing] = useState(false);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const [archive, setArchive] = useState<string>();
  const registry = useRegistry<RouteLocation>('/api/admin/route-locations/list', session, {
    ...filters,
    kind,
  });
  const { items, loading } = registry;
  async function save(event: React.FormEvent) {
    event.preventDefault();
    if (!editor) return;
    setBusy(true);
    setError('');
    try {
      await request<RouteLocation>('/api/admin/route-locations/save', session, {
        ...editor,
        existing: editing,
      });
      setEditor(undefined);
      registry.refresh();
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Не удалось сохранить запись');
    } finally {
      setBusy(false);
    }
  }
  async function archiveLocation(code: string) {
    setBusy(true);
    setError('');
    try {
      await request('/api/admin/route-locations/archive', session, { id: code });
      setArchive(undefined);
      registry.refresh();
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Не удалось архивировать запись');
    } finally {
      setBusy(false);
    }
  }
  return (
    <section className="station-directory">
      <div className="heading">
        <div>
          <p className="eyebrow">ОБЩИЙ СПРАВОЧНИК</p>
          <h1>Порты и аэропорты</h1>
          <p>Справочники доступны всем организациям.</p>
        </div>
        <button
          onClick={() => {
            setEditor({ ...emptyLocation, id: crypto.randomUUID(), kind });
            setEditing(false);
          }}
          disabled={busy}
        >
          Добавить запись
        </button>
      </div>
      <div className="admin-navigation" role="group" aria-label="Тип справочника">
        <button
          aria-pressed={kind === 'sea_port'}
          onClick={() => {
            setKind('sea_port');
            setEditor(undefined);
            setFilters({ sort: 'name', descending: false });
          }}
        >
          Морские порты
        </button>
        <button
          aria-pressed={kind === 'airport'}
          onClick={() => {
            setKind('airport');
            setEditor(undefined);
            setFilters({ sort: 'name', descending: false });
          }}
        >
          Аэропорты
        </button>
      </div>
      <RegistryFilters
        value={filters}
        onApply={setFilters}
        loading={loading}
        placeholder="Название или код…"
      >
        {(draft, change) => (
          <label>
            Страна
            <input
              placeholder="Код страны…"
              maxLength={2}
              pattern="[A-Z]{2}"
              value={draft.country_code ?? ''}
              onChange={(event) =>
                change({ ...draft, country_code: event.target.value.toUpperCase() || undefined })
              }
            />
          </label>
        )}
      </RegistryFilters>
      {error && <p role="alert">{error}</p>}
      {editor && (
        <form className="station-editor" onSubmit={save}>
          <h2>{editing ? 'Редактирование записи' : 'Новая запись'}</h2>
          <div className="station-editor-fields">
            <label>
              Код
              <input
                maxLength={16}
                value={editor.code ?? ''}
                disabled={busy}
                onChange={(event) =>
                  setEditor({ ...editor, code: event.target.value.toUpperCase() || undefined })
                }
              />
            </label>
            <label>
              Название
              <input
                required
                maxLength={256}
                value={editor.name}
                disabled={busy}
                onChange={(event) => setEditor({ ...editor, name: event.target.value })}
              />
            </label>
            <label>
              Код страны
              <input
                required
                pattern="[A-Z]{2}"
                maxLength={2}
                placeholder="Две латинские буквы"
                value={editor.country_code}
                disabled={busy}
                onChange={(event) =>
                  setEditor({ ...editor, country_code: event.target.value.toUpperCase() })
                }
              />
            </label>
          </div>
          <div className="station-editor-actions">
            <button type="button" disabled={busy} onClick={() => setEditor(undefined)}>
              Отменить
            </button>
            <button disabled={busy}>Сохранить</button>
          </div>
        </form>
      )}
      {loading ? (
        <p role="status">Загрузка справочника…</p>
      ) : (
        <>
          <table className="station-table">
            <thead>
              <tr>
                <SortHeading label="Код" field="code" value={filters} onChange={setFilters} />
                <SortHeading label="Название" field="name" value={filters} onChange={setFilters} />
                <SortHeading
                  label="Страна"
                  field="country_code"
                  value={filters}
                  onChange={setFilters}
                />
                <th>
                  <span className="visually-hidden">Действия</span>
                </th>
              </tr>
            </thead>
            <tbody>
              {items.map((location) => (
                <tr
                  key={location.id}
                  tabIndex={0}
                  className="station-row"
                  onClick={() => {
                    setEditor({ ...location });
                    setEditing(true);
                  }}
                  onKeyDown={(event) => {
                    if (
                      event.target === event.currentTarget &&
                      (event.key === 'Enter' || event.key === ' ')
                    ) {
                      event.preventDefault();
                      setEditor({ ...location });
                      setEditing(true);
                    }
                  }}
                >
                  <td>{location.code ?? '—'}</td>
                  <td>
                    <button
                      className="station-name"
                      onClick={() => {
                        setEditor({ ...location });
                        setEditing(true);
                      }}
                    >
                      {location.name}
                    </button>
                  </td>
                  <td>{location.country_code}</td>
                  <td onClick={(event) => event.stopPropagation()}>
                    {archive === location.id ? (
                      <div className="station-editor-actions">
                        <button disabled={busy} onClick={() => archiveLocation(location.id)}>
                          Подтвердить архивирование
                        </button>
                        <button onClick={() => setArchive(undefined)}>Отменить</button>
                      </div>
                    ) : (
                      <button disabled={busy} onClick={() => setArchive(location.id)}>
                        В архив
                      </button>
                    )}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
          {!registry.error && !items.length && (
            <p>
              {filters.query || filters.country_code
                ? 'Записи не найдены.'
                : 'Справочник пока пуст. Добавьте первую запись.'}
            </p>
          )}
          <RegistryMore
            hasMore={registry.hasMore}
            loading={registry.loadingMore}
            error={registry.error}
            loadMore={registry.loadMore}
          />
        </>
      )}
    </section>
  );
}
