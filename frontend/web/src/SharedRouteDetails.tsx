import { useEffect, useState, type ReactNode } from 'react';
import {
  request,
  type Session,
  type SeaDetails,
  type AirDetails,
  type TransportLocation,
} from './api';
import { CustomerSelect, type Counterparty } from './CustomerSelect';
import { TransportLocationLookup } from './TransportLocationLookup';
import { businessDate, calendarDate } from './calendar';

export function SharedRouteFields({
  type,
  sea,
  air,
  session,
  customers,
  onSea,
  onAir,
}: {
  type: string;
  sea?: SeaDetails;
  air?: AirDetails;
  session: Session;
  customers: Counterparty[];
  onSea: (value: SeaDetails) => void;
  onAir: (value: AirDetails) => void;
}) {
  function date(
    label: string,
    value: string | undefined,
    actual: boolean,
    change: (value?: string) => void,
  ) {
    return (
      <label>
        {label}
        <input
          type="date"
          min="1900-01-01"
          max={actual ? businessDate() : '9999-12-31'}
          value={value ?? ''}
          onChange={(event) => change(event.target.value || undefined)}
        />
      </label>
    );
  }
  const company = (label: string, value: string | undefined, change: (id?: string) => void) => (
    <CustomerSelect
      label={label}
      session={session}
      items={customers}
      value={value ?? ''}
      onChange={(id) => change(id || undefined)}
      disabled={false}
    />
  );
  if (type === 'sea') {
    const value = sea ?? {};
    return (
      <div className="shared-route-grid">
        <TransportLocationLookup
          label="POL · порт отправления"
          kind="sea_port"
          value={value.pol}
          session={session}
          onChange={(pol) => onSea({ ...value, pol })}
        />
        <TransportLocationLookup
          label="POD · порт назначения"
          kind="sea_port"
          value={value.pod}
          session={session}
          onChange={(pod) => onSea({ ...value, pod })}
        />
        {company('Терминал отправления', value.departure_terminal_id, (id) =>
          onSea({ ...value, departure_terminal_id: id }),
        )}
        {company('Терминал назначения', value.arrival_terminal_id, (id) =>
          onSea({ ...value, arrival_terminal_id: id }),
        )}
        {date('ETA · ожидаемое прибытие', value.eta, false, (eta) => onSea({ ...value, eta }))}
        {date('Дата выхода судна', value.departure_date, true, (departure_date) =>
          onSea({ ...value, departure_date }),
        )}
        <label>
          Судно
          <input
            maxLength={128}
            value={value.vessel ?? ''}
            onChange={(event) => onSea({ ...value, vessel: event.target.value || undefined })}
          />
        </label>
        <label>
          Рейс
          <input
            maxLength={128}
            value={value.voyage ?? ''}
            onChange={(event) => onSea({ ...value, voyage: event.target.value || undefined })}
          />
        </label>
      </div>
    );
  }
  const value = air ?? {};
  return (
    <div className="shared-route-grid">
      <TransportLocationLookup
        label="Аэропорт отправления"
        kind="airport"
        value={value.origin_airport}
        session={session}
        onChange={(origin_airport) => onAir({ ...value, origin_airport })}
      />
      <TransportLocationLookup
        label="Аэропорт назначения"
        kind="airport"
        value={value.destination_airport}
        session={session}
        onChange={(destination_airport) => onAir({ ...value, destination_airport })}
      />
      {company('Перевозчик', value.carrier_id, (id) => onAir({ ...value, carrier_id: id }))}
      <label>
        Номер рейса
        <input
          maxLength={64}
          value={value.flight_number ?? ''}
          onChange={(event) => onAir({ ...value, flight_number: event.target.value || undefined })}
        />
      </label>
      {date('ETD · ожидаемое отправление', value.etd, false, (etd) => onAir({ ...value, etd }))}
      {date('ETA · ожидаемое прибытие', value.eta, false, (eta) => onAir({ ...value, eta }))}
      {date('Дата убытия', value.departure_date, true, (departure_date) =>
        onAir({ ...value, departure_date }),
      )}
      {date('Дата прибытия', value.arrival_date, true, (arrival_date) =>
        onAir({ ...value, arrival_date }),
      )}
    </div>
  );
}
function CompanyName({
  id,
  customers,
  session,
}: {
  id?: string;
  customers: Counterparty[];
  session?: Session;
}) {
  const known = customers.find((item) => item.id === id)?.name;
  const [name, setName] = useState('');
  useEffect(() => {
    if (!id || known || !session) return;
    const controller = new AbortController();
    request<{ counterparty: Counterparty }>(
      '/api/v1/counterparties/get',
      session,
      { id },
      controller.signal,
    )
      .then((value) => {
        if (!controller.signal.aborted) setName(value.counterparty.name);
      })
      .catch(() => {
        if (!controller.signal.aborted) setName('Контрагент недоступен');
      });
    return () => controller.abort();
  }, [id, known, session]);
  return <>{id ? known || name || 'Загрузка…' : '—'}</>;
}
export function sharedRouteDirection(value: { sea?: SeaDetails; air?: AirDetails }) {
  const from = value.sea?.pol ?? value.air?.origin_airport;
  const to = value.sea?.pod ?? value.air?.destination_airport;
  return `${from?.name || 'Не указан'} → ${to?.name || 'Не указан'}`;
}
export function SharedRouteDetailsView({
  sea,
  air,
  customers,
  session,
}: {
  sea?: SeaDetails;
  air?: AirDetails;
  customers: Counterparty[];
  session?: Session;
}) {
  const location = (value?: TransportLocation) =>
    value ? [value.name, value.code, value.country_code].filter(Boolean).join(' · ') : '—';
  const company = (id?: string) => <CompanyName id={id} customers={customers} session={session} />;
  const rows: [string, ReactNode][] = sea
    ? [
        ['POL · порт отправления', location(sea.pol)],
        ['POD · порт назначения', location(sea.pod)],
        ['Терминал отправления', company(sea.departure_terminal_id)],
        ['Терминал назначения', company(sea.arrival_terminal_id)],
        ['ETA', calendarDate(sea.eta)],
        ['Дата выхода судна', calendarDate(sea.departure_date)],
        ['Судно', sea.vessel || '—'],
        ['Рейс', sea.voyage || '—'],
      ]
    : air
      ? [
          ['Аэропорт отправления', location(air.origin_airport)],
          ['Аэропорт назначения', location(air.destination_airport)],
          ['Перевозчик', company(air.carrier_id)],
          ['Номер рейса', air.flight_number || '—'],
          ['ETD', calendarDate(air.etd)],
          ['ETA', calendarDate(air.eta)],
          ['Дата убытия', calendarDate(air.departure_date)],
          ['Дата прибытия', calendarDate(air.arrival_date)],
        ]
      : [];
  return rows.length ? (
    <dl className="shared-route-values">
      {rows.map(([label, value]) => (
        <div key={label}>
          <dt>{label}</dt>
          <dd>{value}</dd>
        </div>
      ))}
    </dl>
  ) : (
    <p className="muted">Данные участка пока не заполнены.</p>
  );
}
