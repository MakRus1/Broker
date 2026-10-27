import { useEffect, useState } from 'react';
import { request, transportationTypes, type Session, type Transportation } from './api';
import { Chevron, Select } from './Select';
import { initialRegistryQuery, type RegistryQuery } from './transportationRegistry';

export function TransportationFilters({
  session,
  value,
  loading,
  onApply,
}: {
  session: Session;
  value: RegistryQuery;
  loading: boolean;
  onApply: (value: RegistryQuery) => void;
}) {
  const [draft, setDraft] = useState(value);
  const [containerTypes, setContainerTypes] = useState<string[]>([]);
  const [failure, setFailure] = useState('');
  useEffect(() => setDraft(value), [value]);
  useEffect(() => {
    const controller = new AbortController();
    request<{ container_types: string[] }>(
      '/api/v1/shipments/settings',
      session,
      {},
      controller.signal,
    )
      .then((result) => setContainerTypes(result.container_types))
      .catch((error: unknown) => {
        if (!controller.signal.aborted)
          setFailure(
            error instanceof Error ? error.message : 'Не удалось загрузить типы контейнеров',
          );
      });
    return () => controller.abort();
  }, [session]);
  const count =
    (value.query ? 1 : 0) +
    (value.types?.length ? 1 : 0) +
    (value.container_types?.length ? 1 : 0) +
    (value.hazardous !== undefined ? 1 : 0);
  function toggleType(type: Transportation['type']) {
    const next = draft.types?.includes(type)
      ? draft.types.filter((item) => item !== type)
      : [...(draft.types ?? []), type];
    setDraft({ ...draft, types: next.length ? next : undefined });
  }
  return (
    <form
      className="registry-filters"
      onSubmit={(event) => {
        event.preventDefault();
        onApply({ ...draft, query: draft.query?.trim() || undefined });
      }}
    >
      <div className="registry-search">
        <label>
          Номер контейнера
          <input
            type="search"
            value={draft.query ?? ''}
            maxLength={256}
            placeholder="Часть номера…"
            onChange={(event) => setDraft({ ...draft, query: event.target.value })}
          />
        </label>
        <button className="primary" type="submit" disabled={loading}>
          Применить
        </button>
        {count > 0 && (
          <button type="button" disabled={loading} onClick={() => onApply(initialRegistryQuery)}>
            Сбросить
          </button>
        )}
      </div>
      <details>
        <summary>
          <span>Фильтры{count > 0 ? ` · ${count}` : ''}</span>
          <Chevron />
        </summary>
        <div className="registry-filter-fields">
          <fieldset>
            <legend>Тип перевозки</legend>
            <div className="registry-type-options">
              {Object.entries(transportationTypes).map(([type, title]) => (
                <label key={type}>
                  <input
                    type="checkbox"
                    checked={draft.types?.includes(type as Transportation['type']) ?? false}
                    onChange={() => toggleType(type as Transportation['type'])}
                  />
                  {title}
                </label>
              ))}
            </div>
          </fieldset>
          <div>
            <Select
              label="Тип контейнера"
              value={draft.container_types?.[0] ?? ''}
              options={[
                { value: '', label: 'Все типы' },
                ...containerTypes.map((type) => ({ value: type, label: type })),
              ]}
              onChange={(type) =>
                setDraft({ ...draft, container_types: type ? [type] : undefined })
              }
            />
            {failure && (
              <p role="alert" className="field-error">
                {failure}
              </p>
            )}
          </div>
          <Select
            label="Опасный груз"
            value={draft.hazardous === undefined ? '' : String(draft.hazardous)}
            options={[
              { value: '', label: 'Все перевозки' },
              { value: 'true', label: 'Только опасный' },
              { value: 'false', label: 'Без опасного груза' },
            ]}
            onChange={(value) =>
              setDraft({ ...draft, hazardous: value === '' ? undefined : value === 'true' })
            }
          />
        </div>
      </details>
    </form>
  );
}
