import { IsoDateInput } from '../../shared/dates/DateInput';
import { useEffect, useId, useState } from 'react';
import { request } from '../../shared/api/http';
import { type Session } from '../../shared/api/session';
import { type RailDetails, type RailwayStation } from './types';
import { businessDate, calendarDate } from '../../shared/dates/calendar';

function StationLookup({
  label,
  value,
  session,
  onChange,
}: {
  label: string;
  value?: RailwayStation;
  session: Session;
  onChange: (station?: RailwayStation) => void;
}) {
  const id = useId();
  const [query, setQuery] = useState('');
  const [open, setOpen] = useState(false);
  const [items, setItems] = useState<RailwayStation[]>([]);
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
      request<{ items: RailwayStation[] }>(
        '/api/geography/railway-stations',
        session,
        { query: query.trim() },
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
  }, [query, open, session]);
  function choose(station: RailwayStation) {
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
          placeholder="Название станции, ЕСР или UIC"
          value={
            open
              ? query
              : value
                ? [
                    value.name,
                    value.esr_code
                      ? `ЕСР ${value.esr_code}`
                      : value.uic_code
                        ? `UIC ${value.uic_code}`
                        : value.country_code,
                  ].join(' · ')
                : ''
          }
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
            <p role="status">Ищем станции…</p>
          ) : failure ? (
            <p role="alert">{failure}</p>
          ) : !items.length ? (
            <p>Станции не найдены в справочнике</p>
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
                <small>
                  {station.esr_code
                    ? `ЕСР ${station.esr_code}${station.esr_source === 'osm_hint' ? ' (OSM)' : ''} · `
                    : ''}
                  {station.uic_code ? `UIC ${station.uic_code} · ` : ''}
                  {station.country_code}
                </small>
              </button>
            ))
          )}
          {items.length === 20 && <p>Первые 20 совпадений. Уточните название или код.</p>}
          <p>
            <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noreferrer">
              © OpenStreetMap
            </a>{' '}
            · ЕСР из OSM не подтверждён
          </p>
        </div>
      )}
    </div>
  );
}

export const railDates = [
  ['etd', 'ETD · ожидаемое отправление'],
  ['eta', 'ETA · ожидаемое прибытие'],
  ['departure_date', 'Отправление поезда'],
  ['arrival_date', 'Прибытие на станцию назначения'],
  ['unloading_date', 'Выгрузка на станции назначения'],
] as const;
export function RailFields({
  value,
  session,
  onChange,
}: {
  value: RailDetails;
  session: Session;
  onChange: (value: RailDetails) => void;
}) {
  return (
    <>
      <StationLookup
        label="Станция отправления"
        value={value.origin_station}
        session={session}
        onChange={(station) => onChange({ ...value, origin_station: station })}
      />
      <StationLookup
        label="Станция назначения"
        value={value.destination_station}
        session={session}
        onChange={(station) => onChange({ ...value, destination_station: station })}
      />
      {railDates.map(([key, label]) => (
        <label key={key}>
          {label}
          <IsoDateInput
            min="1900-01-01"
            max={key === 'etd' || key === 'eta' ? undefined : businessDate()}
            value={value[key] ?? ''}
            onChange={(dateValue) => onChange({ ...value, [key]: dateValue || undefined })}
          />
        </label>
      ))}
    </>
  );
}
export function RailDetailsView({ value }: { value: RailDetails }) {
  if (!Object.values(value).some(Boolean))
    return <p className="muted">Данные участка пока не указаны.</p>;
  return (
    <dl className="rail-date-grid">
      {railDates.map(([key, label]) => (
        <div key={key}>
          <dt>{label}</dt>
          <dd>{calendarDate(value[key])}</dd>
        </div>
      ))}
    </dl>
  );
}
