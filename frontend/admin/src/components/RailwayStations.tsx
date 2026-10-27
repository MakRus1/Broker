import { useState } from 'react';
import { request, type Session } from '../api';
import { useRegistry, type RegistryQuery } from '../hooks/useRegistry';
import { RegistryFilters, RegistryMore, SortHeading } from './Registry';

type Station = { esr_code: string; name: string; country_code: string };
const emptyStation: Station = { esr_code: '', name: '', country_code: '' };

export function RailwayStations({ session }: { session: Session }) {
  const [filters, setFilters] = useState<RegistryQuery>({ sort: 'name', descending: false });
  const [editor, setEditor] = useState<Station>();
  const [editing, setEditing] = useState(false);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const [archive, setArchive] = useState<string>();
  const registry = useRegistry<Station>('/api/admin/railway-stations/list', session, filters);
  const { items, loading } = registry;
  async function save(event: React.FormEvent) {
    event.preventDefault();
    if (!editor) return;
    setBusy(true);
    setError('');
    try {
      await request<Station>('/api/admin/railway-stations/save', session, {
        ...editor,
        existing: editing,
      });
      setEditor(undefined);
      registry.refresh();
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Не удалось сохранить станцию');
    } finally {
      setBusy(false);
    }
  }
  async function archiveStation(code: string) {
    setBusy(true);
    setError('');
    try {
      await request('/api/admin/railway-stations/archive', session, { esr_code: code });
      setArchive(undefined);
      registry.refresh();
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Не удалось архивировать станцию');
    } finally {
      setBusy(false);
    }
  }
  return (
    <section className="station-directory">
      <div className="heading">
        <div>
          <p className="eyebrow">ОБЩИЙ СПРАВОЧНИК</p>
          <h1>Железнодорожные станции</h1>
          <p>Станции доступны всем организациям.</p>
        </div>
        <button
          onClick={() => {
            setEditor({ ...emptyStation });
            setEditing(false);
          }}
          disabled={busy}
        >
          Добавить станцию
        </button>
      </div>
      <RegistryFilters
        value={filters}
        onApply={setFilters}
        loading={loading}
        placeholder="Название или код ЕСР…"
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
          <h2>{editing ? 'Редактирование станции' : 'Новая станция'}</h2>
          <div className="station-editor-fields">
            <label>
              Код ЕСР
              <input
                required
                pattern="[0-9]{6}"
                maxLength={6}
                inputMode="numeric"
                value={editor.esr_code}
                disabled={editing || busy}
                onChange={(event) => setEditor({ ...editor, esr_code: event.target.value })}
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
        <p role="status">Загрузка станций…</p>
      ) : (
        <>
          <table className="station-table">
            <thead>
              <tr>
                <SortHeading label="ЕСР" field="esr_code" value={filters} onChange={setFilters} />
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
              {items.map((station) => (
                <tr
                  key={station.esr_code}
                  tabIndex={0}
                  className="station-row"
                  onClick={() => {
                    setEditor({ ...station });
                    setEditing(true);
                  }}
                  onKeyDown={(event) => {
                    if (
                      event.target === event.currentTarget &&
                      (event.key === 'Enter' || event.key === ' ')
                    ) {
                      event.preventDefault();
                      setEditor({ ...station });
                      setEditing(true);
                    }
                  }}
                >
                  <td>{station.esr_code}</td>
                  <td>
                    <button
                      className="station-name"
                      onClick={() => {
                        setEditor({ ...station });
                        setEditing(true);
                      }}
                    >
                      {station.name}
                    </button>
                  </td>
                  <td>{station.country_code}</td>
                  <td onClick={(event) => event.stopPropagation()}>
                    {archive === station.esr_code ? (
                      <div className="station-editor-actions">
                        <button disabled={busy} onClick={() => archiveStation(station.esr_code)}>
                          Подтвердить архивирование
                        </button>
                        <button onClick={() => setArchive(undefined)}>Отменить</button>
                      </div>
                    ) : (
                      <button disabled={busy} onClick={() => setArchive(station.esr_code)}>
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
                ? 'Станции не найдены.'
                : 'Справочник пока пуст. Добавьте первую станцию.'}
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
