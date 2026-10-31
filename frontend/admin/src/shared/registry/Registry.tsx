import { useEffect, useRef, useState, type ReactNode } from 'react';
import type { RegistryQuery } from './useRegistry';
import { Select } from '../ui/Select';

export function RegistryFilters({
  value,
  onApply,
  children,
  placeholder,
  loading = false,
}: {
  value: RegistryQuery;
  onApply: (query: RegistryQuery) => void;
  children?: (draft: RegistryQuery, change: (value: RegistryQuery) => void) => ReactNode;
  placeholder: string;
  loading?: boolean;
}) {
  const [draft, setDraft] = useState(value);
  useEffect(() => setDraft(value), [value]);
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
          Поиск
          <input
            type="search"
            maxLength={200}
            placeholder={placeholder}
            value={draft.query ?? ''}
            onChange={(event) => setDraft({ ...draft, query: event.target.value })}
          />
        </label>
      </div>
      {children && (
        <details>
          <summary>Фильтры</summary>
          <div className="registry-filter-fields">{children(draft, setDraft)}</div>
        </details>
      )}
      <div className="registry-filter-actions">
        <button className="primary" disabled={loading}>
          Применить фильтры
        </button>
        {(value.query || value.enabled !== undefined || value.role || value.country_code) && (
          <button
            type="button"
            disabled={loading}
            onClick={() => onApply({ sort: value.sort, descending: value.descending })}
          >
            Сбросить
          </button>
        )}
      </div>
    </form>
  );
}

export function SortHeading({
  label,
  field,
  value,
  onChange,
}: {
  label: string;
  field: string;
  value: RegistryQuery;
  onChange: (value: RegistryQuery) => void;
}) {
  const selected = value.sort === field;
  return (
    <th aria-sort={selected ? (value.descending ? 'descending' : 'ascending') : 'none'}>
      <button
        type="button"
        className="registry-sort"
        onClick={() =>
          onChange({ ...value, sort: field, descending: selected ? !value.descending : false })
        }
      >
        {label}
        <span aria-hidden="true">{selected ? (value.descending ? '↓' : '↑') : '↕'}</span>
      </button>
    </th>
  );
}

export function EnabledFilter({
  value,
  onChange,
}: {
  value?: boolean;
  onChange: (value: boolean | undefined) => void;
}) {
  return (
    <Select
      label="Состояние"
      value={value === undefined ? '' : String(value)}
      options={[
        { value: '', label: 'Все' },
        { value: 'true', label: 'Активные' },
        { value: 'false', label: 'Отключённые' },
      ]}
      onChange={(next) => onChange(next === '' ? undefined : next === 'true')}
    />
  );
}

export function RegistryMore({
  hasMore,
  loading,
  error,
  loadMore,
}: {
  hasMore: boolean;
  loading: boolean;
  error: string;
  loadMore: () => Promise<void>;
}) {
  const sentinel = useRef<HTMLDivElement>(null);
  useEffect(() => {
    if (
      !hasMore ||
      loading ||
      error ||
      !sentinel.current ||
      typeof IntersectionObserver === 'undefined'
    )
      return;
    const observer = new IntersectionObserver(
      (entries) => {
        if (entries.some((entry) => entry.isIntersecting)) void loadMore();
      },
      { rootMargin: '300px' },
    );
    observer.observe(sentinel.current);
    return () => observer.disconnect();
  }, [hasMore, loading, error, loadMore]);
  return (
    <div ref={sentinel} className="registry-more">
      {error && <p role="alert">{error}</p>}
      {loading && <p role="status">Загрузка…</p>}
      {!loading && (error || (hasMore && typeof IntersectionObserver === 'undefined')) && (
        <button onClick={() => void loadMore()}>Загрузить следующую страницу</button>
      )}
    </div>
  );
}
