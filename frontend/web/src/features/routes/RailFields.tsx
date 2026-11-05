import { useLocalization } from '../localization/Localization';
import { IsoDateInput } from '../../shared/dates/DateInput';
import { StationLookup } from '../directories/StationLookup';
import { type Session } from '../../shared/api/session';
import { type RailDetails } from './types';
import { businessDate, calendarDate } from '../../shared/dates/calendar';

export const railDates = [
  ['etd', 'route.etd'],
  ['eta', 'route.eta'],
  ['departure_date', 'forms.trainDeparture'],
  ['arrival_date', 'forms.arrivalAtDestinationStation'],
  ['unloading_date', 'forms.unloadingAtDestinationStation'],
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
  const { translate } = useLocalization();
  return (
    <>
      <StationLookup
        label={translate('registryField.origin_station')}
        value={value.origin_station}
        session={session}
        onChange={(station) => onChange({ ...value, origin_station: station })}
      />
      <StationLookup
        label={translate('registryField.destination_station')}
        value={value.destination_station}
        session={session}
        onChange={(station) => onChange({ ...value, destination_station: station })}
      />
      {railDates.map(([key, label]) => (
        <label key={key}>
          {translate(label)}
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
  const { translate } = useLocalization();
  if (!Object.values(value).some(Boolean))
    return <p className="muted">{translate('route.emptySegment')}</p>;
  return (
    <dl className="rail-date-grid">
      {railDates.map(([key, label]) => (
        <div key={key}>
          <dt>{translate(label)}</dt>
          <dd>{calendarDate(value[key])}</dd>
        </div>
      ))}
    </dl>
  );
}
