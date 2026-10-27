import type { ReactNode } from 'react';
import { Chevron } from './Select';
export function RegistryFilterPanel({
  query,
  onQueryChange,
  onApply,
  onReset,
  count,
  loading,
  label,
  placeholder,
  children,
  columns = 2,
  searchFields,
}: {
  query?: string;
  onQueryChange?: (value: string) => void;
  onApply: () => void;
  onReset: () => void;
  count: number;
  loading: boolean;
  label?: string;
  placeholder?: string;
  children: ReactNode;
  searchFields?: ReactNode;
  columns?: 2 | 3;
}) {
  return (
    <form
      className="registry-filters"
      onSubmit={(event) => {
        event.preventDefault();
        onApply();
      }}
    >
      <div className={`registry-search${searchFields ? ' registry-search--multiple' : ''}`}>
        {searchFields ?? (
          <label>
            {label}
            <input
              type="search"
              value={query ?? ''}
              maxLength={256}
              placeholder={placeholder}
              onChange={(event) => onQueryChange?.(event.target.value)}
            />
          </label>
        )}
        <div className="registry-search-actions">
          <button className="primary" type="submit" disabled={loading}>
            Применить
          </button>
          {count > 0 && (
            <button type="button" disabled={loading} onClick={onReset}>
              Сбросить
            </button>
          )}
        </div>
      </div>
      <details>
        <summary>
          <span>Фильтры{count > 0 ? ` · ${count}` : ''}</span>
          <Chevron />
        </summary>
        <div
          className={`registry-filter-fields${columns === 3 ? ' registry-filter-fields--three' : ''}`}
        >
          {children}
        </div>
      </details>
    </form>
  );
}
