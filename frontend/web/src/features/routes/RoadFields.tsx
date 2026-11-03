import { useLocalization } from '../localization/Localization';
import { IsoDateInput } from '../../shared/dates/DateInput';
import { businessDate, calendarDate } from '../../shared/dates/calendar';
import { CustomerSelect, type Counterparty } from '../counterparties/CustomerSelect';
import { DirectoryLookup } from '../directories/DirectoryLookup';
import type { InternationalRoadDetails, PickupDetails } from './types';
import type { Session } from '../../shared/api/session';

export function RoadFields({
  road,
  pickup,
  international,
  session,
  customers,
  onRoad,
  onPickup,
}: {
  road?: InternationalRoadDetails;
  pickup?: PickupDetails;
  international: boolean;
  session: Session;
  customers: Counterparty[];
  onRoad: (value: InternationalRoadDetails) => void;
  onPickup: (value: PickupDetails) => void;
}) {
  const { translate } = useLocalization();
  const date = (
    label: string,
    value: string | undefined,
    actual: boolean,
    change: (value?: string) => void,
  ) => (
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
  const text = (
    label: string,
    value: string | undefined,
    max: number,
    change: (value?: string) => void,
  ) => (
    <label>
      {label}
      <input
        maxLength={max}
        value={value ?? ''}
        onChange={(event) => change(event.target.value || undefined)}
      />
    </label>
  );
  if (international) {
    const value = road ?? {};
    return (
      <div className="road-details-grid">
        <CustomerSelect
          label={translate('forms.loadingTerminal')}
          items={customers}
          value={value.loading_terminal_id ?? ''}
          onChange={(id) => onRoad({ ...value, loading_terminal_id: id || undefined })}
          session={session}
          disabled={false}
        />
        {text(translate('forms.cmrNumber'), value.cmr_number, 128, (cmr_number) =>
          onRoad({ ...value, cmr_number }),
        )}
        {date(
          translate('forms.plannedLoading'),
          value.planned_loading_date,
          false,
          (planned_loading_date) => onRoad({ ...value, planned_loading_date }),
        )}
        {date(
          translate('forms.plannedUnloading'),
          value.planned_unloading_date,
          false,
          (planned_unloading_date) => onRoad({ ...value, planned_unloading_date }),
        )}
        {date(translate('forms.departureDate'), value.departure_date, true, (departure_date) =>
          onRoad({ ...value, departure_date }),
        )}
        {date(translate('forms.unloadingDate'), value.unloading_date, true, (unloading_date) =>
          onRoad({ ...value, unloading_date }),
        )}
        {text(translate('forms.vehicleNumber'), value.vehicle_number, 64, (vehicle_number) =>
          onRoad({ ...value, vehicle_number }),
        )}
        {text(translate('forms.trailerNumber'), value.trailer_number, 64, (trailer_number) =>
          onRoad({ ...value, trailer_number }),
        )}
      </div>
    );
  }
  const value = pickup ?? {};
  return (
    <div className="road-details-grid">
      {date(
        translate('forms.plannedPickup'),
        value.planned_pickup_date,
        false,
        (planned_pickup_date) => onPickup({ ...value, planned_pickup_date }),
      )}
      {date(
        translate('forms.plannedDelivery'),
        value.planned_delivery_date,
        false,
        (planned_delivery_date) => onPickup({ ...value, planned_delivery_date }),
      )}
      {date(translate('forms.pickupDate'), value.pickup_date, true, (pickup_date) =>
        onPickup({ ...value, pickup_date }),
      )}
      {date(translate('forms.warehouseDelivery'), value.delivery_date, true, (delivery_date) =>
        onPickup({ ...value, delivery_date }),
      )}
      <DirectoryLookup
        label={translate('shipment.deliveryCity')}
        kind="cities"
        session={session}
        value={value.delivery_city ?? ''}
        onChange={(delivery_city, country) =>
          onPickup({
            ...value,
            delivery_city: delivery_city || undefined,
            delivery_country: country ?? value.delivery_country,
          })
        }
      />
      <label>
        {translate('shipment.countryCode')}
        <input
          maxLength={2}
          pattern="[A-Za-z]{2}"
          value={value.delivery_country ?? ''}
          onChange={(event) =>
            onPickup({ ...value, delivery_country: event.target.value.toUpperCase() || undefined })
          }
        />
      </label>
      <label className="road-address">
        {translate('shipment.deliveryAddress')}
        <textarea
          maxLength={1024}
          rows={2}
          value={value.delivery_address ?? ''}
          onChange={(event) =>
            onPickup({ ...value, delivery_address: event.target.value || undefined })
          }
        />
      </label>
      {text(translate('forms.driver'), value.driver, 256, (driver) =>
        onPickup({ ...value, driver }),
      )}
      {text(translate('forms.vehicleNumber'), value.vehicle_number, 64, (vehicle_number) =>
        onPickup({ ...value, vehicle_number }),
      )}
      {date(translate('forms.consignmentNoteDate'), value.ttn_date, true, (ttn_date) =>
        onPickup({ ...value, ttn_date }),
      )}
    </div>
  );
}
export function RoadDetailsView({
  road,
  pickup,
  customers,
}: {
  road?: InternationalRoadDetails;
  pickup?: PickupDetails;
  customers: Counterparty[];
}) {
  const { translate } = useLocalization();
  const company = (id?: string) => customers.find((item) => item.id === id)?.name ?? '—';
  const rows = road
    ? [
        [translate('forms.loadingLocation'), company(road.loading_terminal_id)],
        [translate('forms.cmrNumber'), road.cmr_number],
        [translate('forms.plannedLoading'), calendarDate(road.planned_loading_date)],
        [translate('forms.plannedUnloading'), calendarDate(road.planned_unloading_date)],
        [translate('forms.departureDate'), calendarDate(road.departure_date)],
        [translate('forms.unloadingDate'), calendarDate(road.unloading_date)],
        [translate('forms.vehicleNumber'), road.vehicle_number],
        [translate('forms.trailerNumber'), road.trailer_number],
      ]
    : pickup
      ? [
          [translate('forms.plannedPickup'), calendarDate(pickup.planned_pickup_date)],
          [translate('forms.plannedDelivery'), calendarDate(pickup.planned_delivery_date)],
          [translate('forms.pickupDate'), calendarDate(pickup.pickup_date)],
          [translate('forms.warehouseDelivery'), calendarDate(pickup.delivery_date)],
          [
            translate('shipment.deliveryCity'),
            [pickup.delivery_city, pickup.delivery_country].filter(Boolean).join(' · '),
          ],
          [translate('shipment.deliveryAddress'), pickup.delivery_address],
          [translate('forms.driver'), pickup.driver],
          [translate('forms.vehicleNumber'), pickup.vehicle_number],
          [translate('forms.consignmentNoteDate'), calendarDate(pickup.ttn_date)],
        ]
      : [];
  return Object.values(road ?? pickup ?? {}).some(Boolean) ? (
    <dl className="shared-route-values">
      {rows.map(([label, value]) => (
        <div key={label}>
          <dt>{label}</dt>
          <dd>{value || '—'}</dd>
        </div>
      ))}
    </dl>
  ) : (
    <p className="muted">{translate('route.emptySegment')}</p>
  );
}
