import { Chevron } from './Select';
import { useEffect, useId, useState } from 'react';
import { request, type Session } from './api';
export type { Counterparty } from './counterparties';
import type { Counterparty } from './counterparties';

export function CustomerSelect({
  items,
  value,
  onChange,
  session,
  disabled,
  label = 'Клиент',
}: {
  items: Counterparty[];
  value: string;
  onChange: (id: string) => void;
  session: Session;
  disabled: boolean;
  label?: string;
}) {
  const listId = useId();
  const [query, setQuery] = useState('');
  const [open, setOpen] = useState(false);
  const [active, setActive] = useState(-1);
  const [results, setResults] = useState<Counterparty[]>([]);
  const [selected, setSelected] = useState<Counterparty>();
  const [failure, setFailure] = useState('');
  const [loading, setLoading] = useState(false);
  useEffect(() => {
    if (!open || !session.permissions.includes('counterparties.read')) return;
    const controller = new AbortController();
    setLoading(true);
    setResults([]);
    setActive(-1);
    setFailure('');
    const timer = setTimeout(() => {
      void request<{ items: Counterparty[] }>(
        '/api/v1/counterparties/list',
        session,
        { query: query.trim(), limit: 100, status: 'active' },
        controller.signal,
      )
        .then((result) => {
          if (!controller.signal.aborted) setResults(result.items);
        })
        .catch((error) => {
          if (!controller.signal.aborted) setFailure(error.message);
        })
        .finally(() => {
          if (!controller.signal.aborted) setLoading(false);
        });
    }, 250);
    return () => {
      clearTimeout(timer);
      controller.abort();
    };
  }, [query, open, session]);
  useEffect(() => {
    setSelected((old) => (old?.id === value ? old : undefined));
    if (!value || items.some((item) => item.id === value)) return;
    const controller = new AbortController();
    void request<{ counterparty: Counterparty }>(
      '/api/v1/counterparties/get',
      session,
      { id: value },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) setSelected(result.counterparty);
      })
      .catch(() => {
        /* Недоступную связанную компанию показываем без её данных. */
      });
    return () => controller.abort();
  }, [value, items, session]);
  const current = items.find((item) => item.id === value) ?? selected;
  function choose(item: Counterparty) {
    setSelected(item);
    onChange(item.id);
    setOpen(false);
    setQuery('');
    setActive(-1);
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
      <label htmlFor={`${listId}-input`}>{label}</label>
      <div className="counterparty-combobox">
        <input
          id={`${listId}-input`}
          role="combobox"
          aria-autocomplete="list"
          aria-expanded={open}
          aria-controls={open ? listId : undefined}
          aria-activedescendant={
            open && active >= 0 && results[active] ? `${listId}-${results[active].id}` : undefined
          }
          autoComplete="off"
          disabled={disabled}
          required={label === 'Клиент' && !value}
          value={open ? query : (current?.name ?? (value ? 'Контрагент недоступен' : ''))}
          placeholder="Выберите из справочника или начните вводить название / ИНН"
          onFocus={() => {
            setOpen(true);
            setQuery('');
          }}
          onChange={(event) => {
            if (value) onChange('');
            setQuery(event.target.value);
            setOpen(true);
          }}
          onKeyDown={(event) => {
            if (event.key === 'Escape') {
              if (open) event.stopPropagation();
              event.preventDefault();
              setOpen(false);
              setQuery('');
            }
            if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
              event.preventDefault();
              setOpen(true);
              setActive((old) =>
                results.length
                  ? (old + (event.key === 'ArrowDown' ? 1 : -1) + results.length) % results.length
                  : -1,
              );
            }
            if (event.key === 'Enter' && open) {
              event.preventDefault();
              if (results[active]) choose(results[active]);
            }
          }}
        />
        <button
          type="button"
          className="combobox-toggle"
          aria-label={`Открыть справочник: ${label}`}
          disabled={disabled}
          onClick={() => {
            setOpen((old) => !old);
            setQuery('');
          }}
        >
          <Chevron />
        </button>
      </div>
      {open && (
        <div className="counterparty-options" id={listId} role="listbox" aria-label={label}>
          {loading && <p role="status">Ищем в справочнике…</p>}
          {!loading && !failure && !results.length && (
            <p>Совпадений нет. Измените запрос или добавьте запись в справочнике контрагентов.</p>
          )}
          {failure && (
            <p role="alert" className="error">
              {failure}
            </p>
          )}
          {!loading &&
            results.map((item, index) => (
              <button
                type="button"
                role="option"
                id={`${listId}-${item.id}`}
                key={item.id}
                aria-selected={value === item.id}
                className={active === index ? 'active' : ''}
                onMouseDown={(event) => event.preventDefault()}
                onClick={() => choose(item)}
              >
                <strong>{item.name}</strong>
                <small>{[item.country_code, item.tax_id].filter(Boolean).join(' · ')}</small>
              </button>
            ))}
          {!loading && results.length === 100 && (
            <p>Показаны первые 100 записей. Уточните название или ИНН.</p>
          )}
        </div>
      )}
      {value && (
        <div className="counterparty-selected">
          <span>{current?.name ?? 'Контрагент недоступен'}</span>
          <button
            type="button"
            disabled={disabled}
            onClick={() => {
              onChange('');
              setSelected(undefined);
              setQuery('');
            }}
          >
            Убрать выбор
          </button>
        </div>
      )}
    </div>
  );
}
