import { useState } from 'react';
import { request, type Session } from '../api';
import { useRegistry, type RegistryQuery } from '../hooks/useRegistry';
import { RegistryFilters, RegistryMore, SortHeading } from './Registry';

type Station = {
  id: string;
  esr_code?: string;
  uic_code?: string;
  esr_source?: string;
  name: string;
  country_code: string;
};
const emptyStation: Station = { id: '', name: '', country_code: '' };

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
        id: editor.id,
        name: editor.name,
        country_code: editor.country_code,
        esr_code: editor.esr_code || undefined,
        uic_code: editor.uic_code || undefined,
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
      await request('/api/admin/railway-stations/archive', session, { id: code });
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
          <p>
            Станции доступны всем организациям. Данные © участники{' '}
            <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noreferrer">
              OpenStreetMap
            </a>{' '}
            (ODbL). ЕСР из OSM — неподтверждённая подсказка.
          </p>
        </div>
        <button
          onClick={() => {
            setEditor({ ...emptyStation, id: crypto.randomUUID() });
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
        placeholder="Название, ЕСР или UIC…"
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
                pattern="[0-9]{6}"
                maxLength={6}
                inputMode="numeric"
                value={editor.esr_code ?? ''}
                disabled={busy}
                onChange={(event) => setEditor({ ...editor, esr_code: event.target.value })}
              />
            </label>
            <label>
              Код UIC
              <input
                pattern="[0-9]{7}"
                maxLength={7}
                inputMode="numeric"
                value={editor.uic_code ?? ''}
                disabled={busy}
                onChange={(event) => setEditor({ ...editor, uic_code: event.target.value })}
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
                  key={station.id}
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
                  <td>
                    {station.esr_code ?? '—'}
                    {station.esr_source === 'osm_hint' && <small> · OSM</small>}
                  </td>
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
                    {archive === station.id ? (
                      <div className="station-editor-actions">
                        <button disabled={busy} onClick={() => archiveStation(station.id)}>
                          Подтвердить архивирование
                        </button>
                        <button onClick={() => setArchive(undefined)}>Отменить</button>
                      </div>
                    ) : (
                      <button disabled={busy} onClick={() => setArchive(station.id)}>
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
