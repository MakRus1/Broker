import { IsoDateInput } from '../../shared/dates/DateInput';
import { calendarDate } from '../../shared/dates/calendar';
import { Select } from '../../shared/ui/Select';
import { useLocalization } from '../localization/Localization';
import type { AirWaybill, BillOfLading, RouteBooking, SeaDetails } from './types';

const events = [
  ['original_received', 'route.documents.original'],
  ['telex_release', 'route.documents.telex'],
] as const;

export function RouteDocumentsEditor({
  sea,
  documents = [],
  onDocuments,
  onSea,
}: {
  sea?: SeaDetails;
  documents?: (BillOfLading | AirWaybill)[];
  onDocuments: (documents?: (BillOfLading | AirWaybill)[]) => void;
  onSea?: (sea: SeaDetails) => void;
}) {
  const { translate } = useLocalization();
  const booking = sea?.booking ?? {};
  const updateBooking = (change: Partial<RouteBooking>) => {
    if (!sea || !onSea) return;
    const next = { ...booking, ...change };
    onSea({ ...sea, booking: Object.values(next).some(Boolean) ? next : undefined });
  };
  function updateDocument(index: number, change: Partial<BillOfLading>) {
    onDocuments(
      documents.map((document, position) =>
        position === index ? { ...document, ...change } : document,
      ),
    );
  }
  return (
    <section className="route-documents">
      <h4>{translate(sea ? 'route.documents.bookingTitle' : 'route.documents.title')}</h4>
      {sea && onSea && (
        <div className="record-fields">
          <label>
            <span className="field-label">{translate('route.documents.bookingNumber')}</span>
            <input
              maxLength={128}
              value={booking.number ?? ''}
              onChange={(event) => updateBooking({ number: event.target.value || undefined })}
            />
          </label>
          <Select
            label={translate('route.documents.bookingStatus')}
            value={booking.status ?? ''}
            options={[
              { value: '', label: '—' },
              { value: 'placed', label: translate('route.documents.placed') },
              { value: 'confirmed', label: translate('route.documents.confirmed') },
            ]}
            onChange={(status) =>
              updateBooking({
                status: status === 'placed' || status === 'confirmed' ? status : undefined,
              })
            }
          />
          <label>
            <span className="field-label">{translate('route.documents.placedDate')}</span>
            <IsoDateInput
              value={booking.placed_date ?? ''}
              onChange={(value) => updateBooking({ placed_date: value || undefined })}
            />
          </label>
          <label>
            <span className="field-label">{translate('route.documents.confirmedDate')}</span>
            <IsoDateInput
              value={booking.confirmed_date ?? ''}
              onChange={(value) => updateBooking({ confirmed_date: value || undefined })}
            />
          </label>
          {events.map(([key, label]) => (
            <div className="route-document-event" key={key}>
              <label className="inline-check">
                <input
                  type="checkbox"
                  checked={Boolean(sea[key])}
                  onChange={(event) =>
                    onSea({ ...sea, [key]: event.target.checked ? {} : undefined })
                  }
                />
                {translate(label)}
              </label>
              {sea[key] && (
                <IsoDateInput
                  aria-label={translate(label)}
                  value={sea[key]?.date ?? ''}
                  onChange={(value) => onSea({ ...sea, [key]: { date: value || undefined } })}
                />
              )}
            </div>
          ))}
        </div>
      )}
      <RouteDocumentTable
        sea={Boolean(sea)}
        documents={documents}
        onChange={updateDocument}
        onRemove={(index) => {
          const next = documents.filter((_, position) => position !== index);
          onDocuments(next.length ? next : undefined);
        }}
      />
      <button
        type="button"
        disabled={documents.length >= 20}
        onClick={() =>
          onDocuments([
            ...documents,
            {
              number: '',
            },
          ])
        }
      >
        {translate(sea ? 'route.documents.addBill' : 'route.documents.addAir')}
      </button>
    </section>
  );
}

export function RouteDocumentsView({
  sea,
  documents = [],
}: {
  sea?: SeaDetails;
  documents?: (BillOfLading | AirWaybill)[];
}) {
  const { translate } = useLocalization();
  const booking = sea?.booking;
  if (!booking && !documents.length && !sea?.original_received && !sea?.telex_release) return null;
  return (
    <section className="route-documents">
      <h4>{translate(sea ? 'route.documents.bookingTitle' : 'route.documents.title')}</h4>
      <dl className="shared-route-values">
        {booking?.number && (
          <div>
            <dt>{translate('route.documents.bookingNumber')}</dt>
            <dd>{booking.number}</dd>
          </div>
        )}
        {booking?.status && (
          <div>
            <dt>{translate('route.documents.bookingStatus')}</dt>
            <dd>
              {translate(
                booking.status === 'placed'
                  ? 'route.documents.placed'
                  : 'route.documents.confirmed',
              )}
            </dd>
          </div>
        )}
        {booking?.placed_date && (
          <div>
            <dt>{translate('route.documents.placedDate')}</dt>
            <dd>{calendarDate(booking.placed_date)}</dd>
          </div>
        )}
        {booking?.confirmed_date && (
          <div>
            <dt>{translate('route.documents.confirmedDate')}</dt>
            <dd>{calendarDate(booking.confirmed_date)}</dd>
          </div>
        )}
        {events.map(
          ([key, label]) =>
            sea?.[key] && (
              <div key={key}>
                <dt>{translate(label)}</dt>
                <dd>
                  {sea[key]?.date
                    ? calendarDate(sea[key]?.date)
                    : translate('route.documents.dateUnknown')}
                </dd>
              </div>
            ),
        )}
      </dl>
      <RouteDocumentTable sea={Boolean(sea)} documents={documents} />
    </section>
  );
}

function RouteDocumentTable({
  sea,
  documents,
  onChange,
  onRemove,
}: {
  sea: boolean;
  documents: (BillOfLading | AirWaybill)[];
  onChange?: (index: number, change: Partial<BillOfLading>) => void;
  onRemove?: (index: number) => void;
}) {
  const { translate } = useLocalization();
  const title = translate(sea ? 'route.documents.bills' : 'route.documents.airWaybills');
  const numberLabel = translate(sea ? 'route.documents.billNumber' : 'route.documents.airNumber');
  const legLabel = translate('route.documents.leg');
  const dateLabel = translate('route.documents.date');
  const legs = [
    { value: '', label: '—' },
    { value: 'ocean', label: translate('route.documents.ocean') },
    { value: 'feeder', label: translate('route.documents.feeder') },
  ];
  if (!documents.length) return null;
  return (
    <div className="route-document-table-scroll">
      <table className={onChange ? 'route-document-table is-editing' : 'route-document-table'}>
        <caption>{title}</caption>
        <thead>
          <tr>
            <th scope="col">{numberLabel}</th>
            {sea && <th scope="col">{legLabel}</th>}
            <th scope="col">{dateLabel}</th>
            {onRemove && (
              <th scope="col">
                <span className="visually-hidden">{translate('route.documents.remove')}</span>
              </th>
            )}
          </tr>
        </thead>
        <tbody>
          {documents.map((document, index) => {
            const leg = 'leg' in document ? (document.leg ?? '') : '';
            return (
              <tr key={index}>
                <td>
                  {onChange ? (
                    <input
                      aria-label={numberLabel}
                      required
                      maxLength={128}
                      value={document.number}
                      onChange={(event) => onChange(index, { number: event.target.value })}
                    />
                  ) : (
                    document.number
                  )}
                </td>
                {sea && (
                  <td>
                    {onChange ? (
                      <Select
                        label={legLabel}
                        hideLabel
                        value={leg}
                        options={legs}
                        onChange={(leg) =>
                          onChange(index, {
                            leg: leg === 'ocean' || leg === 'feeder' ? leg : undefined,
                          })
                        }
                      />
                    ) : (
                      legs.find(({ value }) => value === leg)?.label
                    )}
                  </td>
                )}
                <td>
                  {onChange ? (
                    <IsoDateInput
                      aria-label={dateLabel}
                      value={document.date ?? ''}
                      onChange={(value) => onChange(index, { date: value || undefined })}
                    />
                  ) : document.date ? (
                    calendarDate(document.date)
                  ) : (
                    '—'
                  )}
                </td>
                {onRemove && (
                  <td className="route-document-action">
                    <button type="button" onClick={() => onRemove(index)}>
                      {translate('route.documents.remove')}
                    </button>
                  </td>
                )}
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
}
