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
    const crossings = value.planned_border_crossings ?? [];
    const changeCrossings = (planned_border_crossings: string[]) =>
      onRoad({
        ...value,
        planned_border_crossings: planned_border_crossings.length
          ? planned_border_crossings
          : undefined,
      });
    return (
      <div className="road-sections">
        <div className="road-details-grid">
          <CustomerSelect
            label={translate('forms.loadingTerminal')}
            items={customers}
            value={value.loading_terminal_id ?? ''}
            onChange={(id) => onRoad({ ...value, loading_terminal_id: id || undefined })}
            session={session}
            disabled={false}
          />
          <CustomerSelect
            label={translate('forms.unloadingTerminal')}
            items={customers}
            value={value.unloading_terminal_id ?? ''}
            onChange={(id) => onRoad({ ...value, unloading_terminal_id: id || undefined })}
            session={session}
            disabled={false}
          />
          {text(translate('forms.loadingAddress'), value.loading_address, 1024, (loading_address) =>
            onRoad({ ...value, loading_address }),
          )}
          {text(
            translate('forms.unloadingAddress'),
            value.unloading_address,
            1024,
            (unloading_address) => onRoad({ ...value, unloading_address }),
          )}
          <div className="road-customs-terminal">
            <CustomerSelect
              label={translate('forms.customsTerminal')}
              items={customers}
              value={value.customs_terminal_id ?? ''}
              onChange={(id) => onRoad({ ...value, customs_terminal_id: id || undefined })}
              session={session}
              disabled={false}
            />
          </div>
          {date(
            translate('forms.customsArrivalDate'),
            value.customs_arrival_date,
            true,
            (customs_arrival_date) => onRoad({ ...value, customs_arrival_date }),
          )}
          {date(
            translate('forms.customsDepartureDate'),
            value.customs_departure_date,
            true,
            (customs_departure_date) => onRoad({ ...value, customs_departure_date }),
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
        <BorderCrossings crossings={crossings} onChange={changeCrossings} />
        <section className="road-section">
          <h4>{translate('forms.cmrTitle')}</h4>
          <div className="road-details-grid">
            {text(translate('forms.cmrNumber'), value.cmr_number, 128, (cmr_number) =>
              onRoad({ ...value, cmr_number }),
            )}
            {date(
              translate('forms.cmrReceivedDate'),
              value.cmr_received_date,
              true,
              (cmr_received_date) => onRoad({ ...value, cmr_received_date }),
            )}
          </div>
        </section>
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
  international,
  customers,
}: {
  road?: InternationalRoadDetails;
  pickup?: PickupDetails;
  international: boolean;
  customers: Counterparty[];
}) {
  const { translate } = useLocalization();
  const company = (id?: string) => customers.find((item) => item.id === id)?.name;
  const renderRows = (rows: [string, string | undefined][]) => (
    <dl className="shared-route-values">
      {rows.map(([label, value]) => (
        <div key={label}>
          <dt>{label}</dt>
          <dd>{value || translate('forms.valueNotSpecified')}</dd>
        </div>
      ))}
    </dl>
  );
  if (international) {
    const detailsSource = road ?? {};
    const section = (title: string, rows: [string, string | undefined][]) => {
      return (
        <section className="road-section">
          <h4>{title}</h4>
          {renderRows(rows)}
        </section>
      );
    };
    const date = (value?: string) => (value ? calendarDate(value) : undefined);
    const details: [string, string | undefined][] = [
      [
        translate('forms.loadingTerminal'),
        detailsSource.loading_terminal_id && company(detailsSource.loading_terminal_id),
      ],
      [
        translate('forms.unloadingTerminal'),
        detailsSource.unloading_terminal_id && company(detailsSource.unloading_terminal_id),
      ],
      [translate('forms.loadingAddress'), detailsSource.loading_address],
      [translate('forms.unloadingAddress'), detailsSource.unloading_address],
      [
        translate('forms.customsTerminal'),
        detailsSource.customs_terminal_id && company(detailsSource.customs_terminal_id),
      ],
      [translate('forms.customsArrivalDate'), date(detailsSource.customs_arrival_date)],
      [translate('forms.customsDepartureDate'), date(detailsSource.customs_departure_date)],
      [translate('forms.plannedLoading'), date(detailsSource.planned_loading_date)],
      [translate('forms.plannedUnloading'), date(detailsSource.planned_unloading_date)],
      [translate('forms.departureDate'), date(detailsSource.departure_date)],
      [translate('forms.unloadingDate'), date(detailsSource.unloading_date)],
      [translate('forms.vehicleNumber'), detailsSource.vehicle_number],
      [translate('forms.trailerNumber'), detailsSource.trailer_number],
    ];
    return (
      <div className="road-sections">
        {renderRows(details)}
        {detailsSource.planned_border_crossings?.length ? (
          <BorderCrossings crossings={detailsSource.planned_border_crossings} />
        ) : null}
        {section(translate('forms.cmrTitle'), [
          [translate('forms.cmrNumber'), detailsSource.cmr_number],
          [translate('forms.cmrReceivedDate'), date(detailsSource.cmr_received_date)],
        ])}
      </div>
    );
  }
  const pickupDetails = pickup ?? {};
  const date = (value?: string) => (value ? calendarDate(value) : undefined);
  return renderRows([
    [translate('forms.plannedPickup'), date(pickupDetails.planned_pickup_date)],
    [translate('forms.plannedDelivery'), date(pickupDetails.planned_delivery_date)],
    [translate('forms.pickupDate'), date(pickupDetails.pickup_date)],
    [translate('forms.warehouseDelivery'), date(pickupDetails.delivery_date)],
    [
      translate('shipment.deliveryCity'),
      [pickupDetails.delivery_city, pickupDetails.delivery_country].filter(Boolean).join(' · '),
    ],
    [translate('shipment.deliveryAddress'), pickupDetails.delivery_address],
    [translate('forms.driver'), pickupDetails.driver],
    [translate('forms.vehicleNumber'), pickupDetails.vehicle_number],
    [translate('forms.consignmentNoteDate'), date(pickupDetails.ttn_date)],
  ]);
}

function BorderCrossings({
  crossings,
  onChange,
}: {
  crossings: string[];
  onChange?: (crossings: string[]) => void;
}) {
  const { translate } = useLocalization();
  const title = translate('forms.plannedBorderCrossings');
  const name = translate('forms.borderCrossing');
  const rows = crossings.length ? crossings : [''];
  return (
    <section className="road-border-crossings" aria-label={title}>
      <h4>{title}</h4>
      <table className="route-document-table">
        <thead>
          <tr>
            <th scope="col" className="route-document-action">
              №
            </th>
            <th scope="col">{name}</th>
            {onChange && (
              <th scope="col" className="route-document-action">
                <span className="visually-hidden">{translate('forms.removeBorderCrossing')}</span>
              </th>
            )}
          </tr>
        </thead>
        <tbody>
          {rows.map((crossing, index) => {
            const label = `${name} ${index + 1}`;
            return (
              <tr key={index}>
                <td>{index + 1}</td>
                <td>
                  {onChange ? (
                    <input
                      aria-label={label}
                      placeholder={translate('forms.borderCrossingPlaceholder')}
                      maxLength={256}
                      value={crossing}
                      onChange={(event) =>
                        onChange(
                          rows.map((item, position) =>
                            position === index ? event.target.value : item,
                          ),
                        )
                      }
                    />
                  ) : (
                    crossing
                  )}
                </td>
                {onChange && (
                  <td className="route-document-action">
                    <button
                      type="button"
                      aria-label={translate('forms.removeField', { label })}
                      disabled={rows.length === 1 && !crossing}
                      onClick={() =>
                        onChange(crossings.filter((_, position) => position !== index))
                      }
                    >
                      {translate('forms.removeBorderCrossing')}
                    </button>
                  </td>
                )}
              </tr>
            );
          })}
        </tbody>
      </table>
      {onChange && (
        <button type="button" disabled={rows.length >= 32} onClick={() => onChange([...rows, ''])}>
          {translate('forms.addBorderCrossing')}
        </button>
      )}
    </section>
  );
}
