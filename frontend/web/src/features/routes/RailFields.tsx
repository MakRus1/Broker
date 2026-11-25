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
  const missing = translate('forms.valueNotSpecified');
  return (
    <dl className="rail-date-grid">
      {(
        [
          ['registryField.origin_station', value.origin_station],
          ['registryField.destination_station', value.destination_station],
        ] as const
      ).map(([label, station]) => (
        <div key={label}>
          <dt>{translate(label)}</dt>
          <dd>
            {station
              ? [
                  station.name,
                  station.esr_code
                    ? translate('forms.esrCode', { code: station.esr_code })
                    : station.uic_code && `UIC ${station.uic_code}`,
                ]
                  .filter(Boolean)
                  .join(' · ')
              : missing}
          </dd>
        </div>
      ))}
      {railDates.map(([key, label]) => (
        <div key={key}>
          <dt>{translate(label)}</dt>
          <dd>{value[key] ? calendarDate(value[key]) : missing}</dd>
        </div>
      ))}
    </dl>
  );
}
