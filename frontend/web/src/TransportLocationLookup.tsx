import { useEffect, useId, useState } from 'react';
import { request, type Session, type TransportLocation, type TransportLocationKind } from './api';

export function TransportLocationLookup({
  label,
  kind,
  value,
  session,
  onChange,
}: {
  label: string;
  kind: TransportLocationKind;
  value?: TransportLocation;
  session: Session;
  onChange: (station?: TransportLocation) => void;
}) {
  const id = useId();
  const [query, setQuery] = useState('');
  const [open, setOpen] = useState(false);
  const [items, setItems] = useState<TransportLocation[]>([]);
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const [active, setActive] = useState(-1);
  useEffect(() => {
    if (!open) {
      setItems([]);
      setBusy(false);
      return;
    }
    const controller = new AbortController();
    setBusy(true);
    setFailure('');
    setItems([]);
    setActive(-1);
    const timer = setTimeout(() => {
      request<{ items: TransportLocation[] }>(
        '/api/geography/route-locations',
        session,
        { kind, query: query.trim() },
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
    }, 250);
    return () => {
      clearTimeout(timer);
      controller.abort();
    };
  }, [query, open, session, kind]);
  function choose(station: TransportLocation) {
    onChange(station);
    setOpen(false);
    setQuery('');
  }
  return (
    <div
      className="station-lookup"
      onBlur={(event) => {
        if (!event.currentTarget.contains(event.relatedTarget)) {
          setOpen(false);
          setQuery('');
        }
      }}
    >
      <label htmlFor={id}>{label}</label>
      <div className="station-input">
        <input
          id={id}
          role="combobox"
          aria-expanded={open}
          aria-autocomplete="list"
          aria-controls={open ? `${id}-options` : undefined}
          aria-activedescendant={open && items[active] ? `${id}-${items[active].id}` : undefined}
          autoComplete="off"
          placeholder="Название или код"
          value={open ? query : value ? [value.name, value.code].filter(Boolean).join(' · ') : ''}
          onFocus={() => {
            setOpen(true);
            setQuery('');
          }}
          onChange={(event) => {
            setQuery(event.target.value);
            setOpen(true);
          }}
          onKeyDown={(event) => {
            if (event.key === 'Escape') {
              event.preventDefault();
              event.stopPropagation();
              setOpen(false);
            }
            if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
              event.preventDefault();
              setOpen(true);
              setActive((old) =>
                items.length
                  ? (old + (event.key === 'ArrowDown' ? 1 : -1) + items.length) % items.length
                  : -1,
              );
            }
            if (event.key === 'Enter' && open) {
              event.preventDefault();
              if (items[active]) choose(items[active]);
            }
          }}
        />
        {value && (
          <button
            type="button"
            aria-label={`Убрать: ${label}`}
            onClick={() => {
              onChange(undefined);
              setQuery('');
            }}
          >
            ×
          </button>
        )}
      </div>
      {open && (
        <div
          className="counterparty-options"
          id={`${id}-options`}
          role="listbox"
          aria-label={label}
        >
          {busy ? (
            <p role="status">Ищем…</p>
          ) : failure ? (
            <p role="alert">{failure}</p>
          ) : !items.length ? (
            <p>Совпадения не найдены в справочнике</p>
          ) : (
            items.map((station, index) => (
              <button
                type="button"
                role="option"
                id={`${id}-${station.id}`}
                key={station.id}
                aria-selected={value?.id === station.id}
                className={active === index ? 'active' : ''}
                onMouseDown={(event) => event.preventDefault()}
                onClick={() => choose(station)}
              >
                {station.name}
                <small>{[station.code, station.country_code].filter(Boolean).join(' · ')}</small>
              </button>
            ))
          )}
          {items.length === 20 && <p>Первые 20 совпадений. Уточните название или код.</p>}
        </div>
      )}
    </div>
  );
}
