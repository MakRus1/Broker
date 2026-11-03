import { translate as translateMessage, type Locale } from '../../../../shared/i18n/catalog';
import { useLocalization } from '../localization/Localization';
import { IsoDateInput } from '../../shared/dates/DateInput';
import { useEffect, useState, type ReactNode } from 'react';
import { request } from '../../shared/api/http';
import { type Session } from '../../shared/api/session';
import { type SeaDetails, type AirDetails, type TransportLocation } from './types';
import { CustomerSelect, type Counterparty } from '../counterparties/CustomerSelect';
import { TransportLocationLookup } from '../directories/TransportLocationLookup';
import { businessDate, calendarDate } from '../../shared/dates/calendar';

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
  const { translate } = useLocalization();
  function date(
    label: string,
    value: string | undefined,
    actual: boolean,
    change: (value?: string) => void,
  ) {
    return (
      <label>
        {label}
        <IsoDateInput
          min="1900-01-01"
          max={actual ? businessDate() : '9999-12-31'}
          value={value ?? ''}
          onChange={(dateValue) => change(dateValue || undefined)}
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
          label={translate('shipment.pol')}
          kind="sea_port"
          value={value.pol}
          session={session}
          onChange={(pol) => onSea({ ...value, pol })}
        />
        <TransportLocationLookup
          label={translate('shipment.pod')}
          kind="sea_port"
          value={value.pod}
          session={session}
          onChange={(pod) => onSea({ ...value, pod })}
        />
        {company(translate('route.departureTerminal'), value.departure_terminal_id, (id) =>
          onSea({ ...value, departure_terminal_id: id }),
        )}
        {company(translate('route.arrivalTerminal'), value.arrival_terminal_id, (id) =>
          onSea({ ...value, arrival_terminal_id: id }),
        )}
        {date(translate('route.eta'), value.eta, false, (eta) => onSea({ ...value, eta }))}
        {date(translate('route.vesselDeparture'), value.departure_date, true, (departure_date) =>
          onSea({ ...value, departure_date }),
        )}
        <label>
          {translate('registryValue.type.vessel')}
          <input
            maxLength={128}
            value={value.vessel ?? ''}
            onChange={(event) => onSea({ ...value, vessel: event.target.value || undefined })}
          />
        </label>
        <label>
          {translate('route.voyage')}
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
        label={translate('registryField.origin_airport')}
        kind="airport"
        value={value.origin_airport}
        session={session}
        onChange={(origin_airport) => onAir({ ...value, origin_airport })}
      />
      <TransportLocationLookup
        label={translate('registryField.destination_airport')}
        kind="airport"
        value={value.destination_airport}
        session={session}
        onChange={(destination_airport) => onAir({ ...value, destination_airport })}
      />
      {company(translate('registryValue.roles.carrier'), value.carrier_id, (id) =>
        onAir({ ...value, carrier_id: id }),
      )}
      <label>
        {translate('shipment.flight')}
        <input
          maxLength={64}
          value={value.flight_number ?? ''}
          onChange={(event) => onAir({ ...value, flight_number: event.target.value || undefined })}
        />
      </label>
      {date(translate('route.etd'), value.etd, false, (etd) => onAir({ ...value, etd }))}
      {date(translate('route.eta'), value.eta, false, (eta) => onAir({ ...value, eta }))}
      {date(translate('route.departureDate'), value.departure_date, true, (departure_date) =>
        onAir({ ...value, departure_date }),
      )}
      {date(translate('route.arrivalDate'), value.arrival_date, true, (arrival_date) =>
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
  const { translate, locale } = useLocalization();
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
        if (!controller.signal.aborted) setName(translate('lookup.counterpartyUnavailable'));
      });
    return () => controller.abort();
  }, [id, known, session, locale]);
  return <>{id ? known || name || translate('filter.loading') : '—'}</>;
}
export function sharedRouteDirection(
  value: { sea?: SeaDetails; air?: AirDetails },
  locale: Locale,
) {
  const from = value.sea?.pol ?? value.air?.origin_airport;
  const to = value.sea?.pod ?? value.air?.destination_airport;
  return `${from?.name || translateMessage(locale, 'routeFilter.pointNotSet')} → ${to?.name || translateMessage(locale, 'routeFilter.pointNotSet')}`;
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
  const { translate } = useLocalization();
  const location = (value?: TransportLocation) =>
    value ? [value.name, value.code, value.country_code].filter(Boolean).join(' · ') : '—';
  const company = (id?: string) => <CompanyName id={id} customers={customers} session={session} />;
  const rows: [string, ReactNode][] = sea
    ? [
        [translate('shipment.pol'), location(sea.pol)],
        [translate('shipment.pod'), location(sea.pod)],
        [translate('route.departureTerminal'), company(sea.departure_terminal_id)],
        [translate('route.arrivalTerminal'), company(sea.arrival_terminal_id)],
        ['ETA', calendarDate(sea.eta)],
        [translate('route.vesselDeparture'), calendarDate(sea.departure_date)],
        [translate('registryValue.type.vessel'), sea.vessel || '—'],
        [translate('route.voyage'), sea.voyage || '—'],
      ]
    : air
      ? [
          [translate('registryField.origin_airport'), location(air.origin_airport)],
          [translate('registryField.destination_airport'), location(air.destination_airport)],
          [translate('registryValue.roles.carrier'), company(air.carrier_id)],
          [translate('shipment.flight'), air.flight_number || '—'],
          ['ETD', calendarDate(air.etd)],
          ['ETA', calendarDate(air.eta)],
          [translate('route.departureDate'), calendarDate(air.departure_date)],
          [translate('route.arrivalDate'), calendarDate(air.arrival_date)],
        ]
      : [];
  return Object.values(sea ?? air ?? {}).some(Boolean) ? (
    <dl className="shared-route-values">
      {rows.map(([label, value]) => (
        <div key={label}>
          <dt>{label}</dt>
          <dd>{value}</dd>
        </div>
      ))}
    </dl>
  ) : (
    <p className="muted">{translate('route.emptySegment')}</p>
  );
}
