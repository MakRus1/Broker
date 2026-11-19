import type { RoutePlan as Plan } from './types';
import type { Session } from '../../shared/api/session';
import { useLocalization } from '../localization/Localization';
import { DirectoryLookup } from '../directories/DirectoryLookup';
import { IsoDateInput } from '../../shared/dates/DateInput';
import { calendarDate } from '../../shared/dates/calendar';

export function RoutePlanFields({
  value,
  session,
  onChange,
}: {
  value: Plan;
  session: Session;
  onChange: (value: Plan) => void;
}) {
  const { translate } = useLocalization();
  return (
    <div className="shared-route-grid">
      {(['origin', 'destination'] as const).map((point) => (
        <div key={point} className="route-plan-point">
          <h4>{translate(point === 'origin' ? 'route.planOrigin' : 'route.planDestination')}</h4>
          <DirectoryLookup
            label={translate('route.planPoint')}
            kind="cities"
            session={session}
            value={value[`${point}_name`] ?? ''}
            onChange={(name, country) =>
              onChange({
                ...value,
                [`${point}_name`]: name || undefined,
                [`${point}_country`]: country ?? value[`${point}_country`],
              })
            }
          />
          <label>
            {translate('shipment.countryCode')}
            <input
              maxLength={2}
              pattern="[A-Z]{2}"
              value={value[`${point}_country`] ?? ''}
              onChange={(event) =>
                onChange({
                  ...value,
                  [`${point}_country`]: event.target.value.toUpperCase() || undefined,
                })
              }
            />
          </label>
          <label>
            {translate('route.planAddress')}
            <textarea
              rows={2}
              maxLength={1024}
              value={value[`${point}_address`] ?? ''}
              onChange={(event) =>
                onChange({ ...value, [`${point}_address`]: event.target.value || undefined })
              }
            />
          </label>
          <label>
            {translate(point === 'origin' ? 'route.planDeparture' : 'route.planArrival')}
            <IsoDateInput
              min={point === 'destination' ? (value.departure_date ?? '1900-01-01') : '1900-01-01'}
              max="9999-12-31"
              value={value[point === 'origin' ? 'departure_date' : 'arrival_date'] ?? ''}
              onChange={(date) =>
                onChange({
                  ...value,
                  [point === 'origin' ? 'departure_date' : 'arrival_date']: date || undefined,
                })
              }
            />
          </label>
        </div>
      ))}
    </div>
  );
}

export function RoutePlanView({ value }: { value?: Plan }) {
  const { translate } = useLocalization();
  if (!value || !Object.values(value).some(Boolean))
    return <p className="hint">{translate('route.planEmpty')}</p>;
  return (
    <dl className="shared-route-values">
      {(['origin', 'destination'] as const).map((point) => (
        <div key={point}>
          <dt>{translate(point === 'origin' ? 'route.planOrigin' : 'route.planDestination')}</dt>
          <dd>
            {[value[`${point}_name`], value[`${point}_country`], value[`${point}_address`]]
              .filter(Boolean)
              .join(' · ') || '—'}
          </dd>
        </div>
      ))}
      <div>
        <dt>{translate('route.planDeparture')}</dt>
        <dd>{calendarDate(value.departure_date)}</dd>
      </div>
      <div>
        <dt>{translate('route.planArrival')}</dt>
        <dd>{calendarDate(value.arrival_date)}</dd>
      </div>
    </dl>
  );
}
