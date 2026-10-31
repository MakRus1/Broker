import { useEffect, useId, useState } from 'react';
import { request } from '../../shared/api/http';
import { type Session } from '../../shared/api/session';

type Choice = { id: string; label: string; value: string; country?: string };
export function DirectoryLookup({
  session,
  kind,
  value,
  onChange,
  label,
}: {
  label: string;
  session: Session;
  kind: 'employees' | 'cities';
  value: string;
  onChange: (value: string, country?: string) => void;
}) {
  const id = useId();
  const [open, setOpen] = useState(false);
  const [query, setQuery] = useState('');
  const [name, setName] = useState('');
  const [items, setItems] = useState<Choice[]>([]);
  const [failure, setFailure] = useState('');
  const [loading, setLoading] = useState(false);
  const [active, setActive] = useState(-1);
  const employees = kind === 'employees';
  useEffect(() => {
    if (!employees || !value) {
      setName('');
      return;
    }
    const controller = new AbortController();
    request<{ items: { subject: string; display_name: string }[] }>(
      '/api/organizations/employees/list',
      session,
      { subject: value, limit: 1 },
      controller.signal,
    )
      .then(({ items }) => {
        if (!controller.signal.aborted) setName(items[0]?.display_name ?? 'Сотрудник недоступен');
      })
      .catch(() => {
        if (!controller.signal.aborted) setName('Не удалось загрузить сотрудника');
      });
    return () => controller.abort();
  }, [employees, value, session]);
  useEffect(() => {
    if (!open || (!employees && query.trim().length < 2)) {
      setItems([]);
      setLoading(false);
      setFailure('');
      return;
    }
    const controller = new AbortController();
    setItems([]);
    setActive(-1);
    setFailure('');
    setLoading(true);
    const timer = setTimeout(() => {
      request<{
        items: (
          | { subject: string; display_name: string }
          | { name: string; label: string; country_code: string }
        )[];
      }>(
        employees ? '/api/organizations/employees/list' : '/api/geography/cities',
        session,
        { query: query.trim() },
        controller.signal,
      )
        .then(({ items }) => {
          if (!controller.signal.aborted)
            setItems(
              items.map((item) =>
                'subject' in item
                  ? { id: item.subject, value: item.subject, label: item.display_name }
                  : {
                      id: `${item.country_code}:${item.label}`,
                      value: item.name,
                      label: item.label,
                      country: item.country_code,
                    },
              ),
            );
        })
        .catch((error) => {
          if (!controller.signal.aborted) setFailure(error.message);
        })
        .finally(() => {
          if (!controller.signal.aborted) setLoading(false);
        });
    }, 300);
    return () => {
      clearTimeout(timer);
      controller.abort();
    };
  }, [open, query, kind, employees, session]);
  function choose(item: Choice) {
    setName(item.label);
    onChange(item.value, item.country);
    setOpen(false);
    setQuery('');
  }
  return (
    <div
      className="directory-lookup"
      onBlur={(event) => {
        if (!event.currentTarget.contains(event.relatedTarget)) {
          setOpen(false);
          setQuery('');
        }
      }}
    >
      <label htmlFor={id}>{label}</label>
      <div className="directory-lookup-control">
        <input
          id={id}
          role="combobox"
          aria-autocomplete="list"
          aria-expanded={open}
          aria-controls={open ? `${id}-results` : undefined}
          aria-activedescendant={
            open && active >= 0 && items[active] ? `${id}-${active}` : undefined
          }
          className={employees && value ? 'has-clear' : undefined}
          maxLength={200}
          value={open ? query : employees ? name : value}
          placeholder={employees ? 'Выберите сотрудника или введите ФИО' : 'Введите город'}
          onFocus={() => {
            setOpen(true);
            setQuery(employees ? '' : value);
          }}
          onChange={(event) => {
            setQuery(event.target.value);
            setOpen(true);
            if (!employees) onChange(event.target.value);
          }}
          onKeyDown={(event) => {
            if (event.key === 'Escape') {
              event.preventDefault();
              setOpen(false);
            }
            if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
              event.preventDefault();
              setOpen(true);
              setActive((old) =>
                Math.max(0, Math.min(items.length - 1, old + (event.key === 'ArrowDown' ? 1 : -1))),
              );
            }
            if (event.key === 'Enter' && open && active >= 0 && items[active]) {
              event.preventDefault();
              choose(items[active]);
            }
          }}
        />
        {employees && value && (
          <button
            type="button"
            className="directory-lookup-clear"
            aria-label={`Очистить поле «${label}»`}
            onClick={() => {
              onChange('');
              setName('');
              setOpen(false);
            }}
          >
            ×
          </button>
        )}
      </div>
      {open && (employees || query.trim().length >= 2) && (
        <div
          className="cargo-suggestions shipment-lookup-options"
          id={`${id}-results`}
          role="listbox"
        >
          {loading && <p role="status">Ищем…</p>}
          {failure && <p role="status">{failure}</p>}
          {!loading && !failure && !items.length && (
            <p>Совпадений нет{employees ? '' : '. Можно ввести город и страну вручную'}.</p>
          )}
          {items.map((item, index) => (
            <button
              key={item.id}
              id={`${id}-${index}`}
              type="button"
              role="option"
              aria-selected={active === index}
              onClick={() => choose(item)}
            >
              {item.label}
            </button>
          ))}
          {!employees && <small>Подсказки DaData</small>}
        </div>
      )}
    </div>
  );
}
