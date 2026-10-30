import { useEffect, useId, useRef, useState } from 'react';
import { request, type Session } from './api';
import { Chevron } from './Select';
import type { FinancialLabel } from './financialLabels';
import type { Contract } from './ContractsPage';

// Эти два справочника используют одинаковый поиск, но сохраняют тип выбранной модели.
export function ServiceChargeLookup<T extends FinancialLabel | Contract>({
  session,
  kind,
  label,
  value,
  name,
  counterparty,
  disabled,
  required,
  onChange,
}: {
  session: Session;
  kind: 'labels' | 'contracts';
  label: string;
  value: string;
  name: string;
  counterparty?: string;
  disabled: boolean;
  required?: boolean;
  onChange: (value: T | null) => void;
}) {
  const id = useId();
  const input = useRef<HTMLInputElement>(null);
  const [open, setOpen] = useState(false);
  const [query, setQuery] = useState('');
  const [items, setItems] = useState<T[]>([]);
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const [active, setActive] = useState(-1);
  useEffect(
    () =>
      input.current?.setCustomValidity(required && !value ? 'Выберите запись из справочника' : ''),
    [required, value],
  );
  useEffect(() => {
    if (!open) return;
    const controller = new AbortController();
    setBusy(true);
    setItems([]);
    setFailure('');
    setActive(-1);
    const timer = setTimeout(() => {
      const field = kind === 'labels' ? 'name' : 'number';
      request<{ items: T[] }>(
        kind === 'labels' ? '/api/v1/financial-labels/list' : '/api/v1/contracts/list',
        session,
        {
          sort: field,
          limit: 50,
          filters: [
            { field: 'archived', values: ['false'] },
            ...(query.trim() ? [{ field, query: query.trim() }] : []),
            ...(kind === 'contracts' && counterparty
              ? [{ field: 'counterparty_id', values: [counterparty] }]
              : []),
          ],
        },
        controller.signal,
      )
        .then((result) => {
          if (!controller.signal.aborted) setItems(result.items);
        })
        .catch((error) => {
          if (!controller.signal.aborted) setFailure(error.message);
        })
        .finally(() => {
          if (!controller.signal.aborted) setBusy(false);
        });
    }, 200);
    return () => {
      clearTimeout(timer);
      controller.abort();
    };
  }, [open, query, session, kind, counterparty]);
  const title = (item: T) => ('name' in item ? item.name : item.number);
  function choose(item: T) {
    onChange(item);
    setOpen(false);
    setQuery('');
  }
  return (
    <div
      className="counterparty-picker"
      onBlur={(event) => {
        if (!event.currentTarget.contains(event.relatedTarget)) {
          setOpen(false);
          setQuery('');
        }
      }}
    >
      <label htmlFor={id}>{label}</label>
      <div className="counterparty-combobox">
        <input
          id={id}
          ref={input}
          role="combobox"
          autoComplete="off"
          disabled={disabled}
          required={required && !value}
          aria-expanded={open}
          aria-controls={open ? `${id}-list` : undefined}
          aria-autocomplete="list"
          aria-activedescendant={open && items[active] ? `${id}-${items[active].id}` : undefined}
          value={open ? query : name}
          placeholder="Выберите из справочника или начните вводить"
          onFocus={() => {
            setOpen(true);
            setQuery('');
          }}
          onChange={(e) => {
            setQuery(e.target.value);
            setOpen(true);
          }}
          onKeyDown={(e) => {
            if (e.key === 'Escape') {
              e.preventDefault();
              e.stopPropagation();
              setOpen(false);
            }
            if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
              e.preventDefault();
              setOpen(true);
              setActive((old) =>
                items.length
                  ? (old + (e.key === 'ArrowDown' ? 1 : -1) + items.length) % items.length
                  : -1,
              );
            }
            if (e.key === 'Enter' && open) {
              e.preventDefault();
              if (items[active]) choose(items[active]);
            }
          }}
        />
        <button
          className="combobox-toggle"
          type="button"
          disabled={disabled}
          aria-label={`Открыть справочник: ${label}`}
          onClick={() => {
            setOpen((old) => !old);
            setQuery('');
          }}
        >
          <Chevron />
        </button>
      </div>
      {open && (
        <div className="counterparty-options" id={`${id}-list`} role="listbox" aria-label={label}>
          {busy && <p role="status">Загружаем…</p>}
          {failure && (
            <p role="alert" className="error">
              {failure}
            </p>
          )}
          {!busy && !failure && !items.length && <p>Совпадений нет</p>}
          {!busy &&
            items.map((item, index) => (
              <button
                type="button"
                role="option"
                id={`${id}-${item.id}`}
                key={item.id}
                aria-selected={item.id === value}
                className={index === active ? 'active' : ''}
                onMouseDown={(e) => e.preventDefault()}
                onClick={() => choose(item)}
              >
                {title(item)}
              </button>
            ))}
          {!busy && items.length === 50 && <p>Первые 50 записей. Уточните поиск.</p>}
        </div>
      )}
      {value && !required && (
        <div className="counterparty-selected">
          <button type="button" disabled={disabled} onClick={() => onChange(null)}>
            Убрать выбор
          </button>
        </div>
      )}
    </div>
  );
}
