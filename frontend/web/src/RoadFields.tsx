import { businessDate, calendarDate } from './calendar';
import { CustomerSelect, type Counterparty } from './CustomerSelect';
import { DirectoryLookup } from './DirectoryLookup';
import type { InternationalRoadDetails, PickupDetails, Session } from './api';

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
  const date = (
    label: string,
    value: string | undefined,
    actual: boolean,
    change: (value?: string) => void,
  ) => (
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
          label="Место погрузки · терминал"
          items={customers}
          value={value.loading_terminal_id ?? ''}
          onChange={(id) => onRoad({ ...value, loading_terminal_id: id || undefined })}
          session={session}
          disabled={false}
        />
        {text('Номер CMR', value.cmr_number, 128, (cmr_number) => onRoad({ ...value, cmr_number }))}
        {date('Плановая погрузка', value.planned_loading_date, false, (planned_loading_date) =>
          onRoad({ ...value, planned_loading_date }),
        )}
        {date('Плановая выгрузка', value.planned_unloading_date, false, (planned_unloading_date) =>
          onRoad({ ...value, planned_unloading_date }),
        )}
        {date('Дата выхода', value.departure_date, true, (departure_date) =>
          onRoad({ ...value, departure_date }),
        )}
        {date('Дата выгрузки', value.unloading_date, true, (unloading_date) =>
          onRoad({ ...value, unloading_date }),
        )}
        {text('Номер машины', value.vehicle_number, 64, (vehicle_number) =>
          onRoad({ ...value, vehicle_number }),
        )}
        {text('Номер прицепа', value.trailer_number, 64, (trailer_number) =>
          onRoad({ ...value, trailer_number }),
        )}
      </div>
    );
  }
  const value = pickup ?? {};
  return (
    <div className="road-details-grid">
      {date('Плановый вывоз', value.planned_pickup_date, false, (planned_pickup_date) =>
        onPickup({ ...value, planned_pickup_date }),
      )}
      {date('Плановая доставка', value.planned_delivery_date, false, (planned_delivery_date) =>
        onPickup({ ...value, planned_delivery_date }),
      )}
      {date('Дата вывоза', value.pickup_date, true, (pickup_date) =>
        onPickup({ ...value, pickup_date }),
      )}
      {date('Доставка на склад', value.delivery_date, true, (delivery_date) =>
        onPickup({ ...value, delivery_date }),
      )}
      <DirectoryLookup
        label="Город доставки"
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
        Код страны
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
        Адрес доставки
        <textarea
          maxLength={1024}
          rows={2}
          value={value.delivery_address ?? ''}
          onChange={(event) =>
            onPickup({ ...value, delivery_address: event.target.value || undefined })
          }
        />
      </label>
      {text('Водитель', value.driver, 256, (driver) => onPickup({ ...value, driver }))}
      {text('Номер машины', value.vehicle_number, 64, (vehicle_number) =>
        onPickup({ ...value, vehicle_number }),
      )}
      {date('Дата ТТН', value.ttn_date, true, (ttn_date) => onPickup({ ...value, ttn_date }))}
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
  const company = (id?: string) => customers.find((item) => item.id === id)?.name ?? '—';
  const rows = road
    ? [
        ['Место погрузки', company(road.loading_terminal_id)],
        ['Номер CMR', road.cmr_number],
        ['Плановая погрузка', calendarDate(road.planned_loading_date)],
        ['Плановая выгрузка', calendarDate(road.planned_unloading_date)],
        ['Дата выхода', calendarDate(road.departure_date)],
        ['Дата выгрузки', calendarDate(road.unloading_date)],
        ['Номер машины', road.vehicle_number],
        ['Номер прицепа', road.trailer_number],
      ]
    : pickup
      ? [
          ['Плановый вывоз', calendarDate(pickup.planned_pickup_date)],
          ['Плановая доставка', calendarDate(pickup.planned_delivery_date)],
          ['Дата вывоза', calendarDate(pickup.pickup_date)],
          ['Доставка на склад', calendarDate(pickup.delivery_date)],
          [
            'Город доставки',
            [pickup.delivery_city, pickup.delivery_country].filter(Boolean).join(' · '),
          ],
          ['Адрес доставки', pickup.delivery_address],
          ['Водитель', pickup.driver],
          ['Номер машины', pickup.vehicle_number],
          ['Дата ТТН', calendarDate(pickup.ttn_date)],
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
    <p className="muted">Данные участка пока не указаны.</p>
  );
}
