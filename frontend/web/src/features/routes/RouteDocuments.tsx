import { IsoDateInput } from '../../shared/dates/DateInput';
import { calendarDate } from '../../shared/dates/calendar';
import { Select } from '../../shared/ui/Select';
import { useLocalization } from '../localization/Localization';
import type { RouteBooking, RouteDocument, SeaDetails } from './types';

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
  documents?: RouteDocument[];
  onDocuments: (documents?: RouteDocument[]) => void;
  onSea?: (sea: SeaDetails) => void;
}) {
  const { translate } = useLocalization();
  const booking = sea?.booking ?? {};
  const updateBooking = (change: Partial<RouteBooking>) => {
    if (!sea || !onSea) return;
    const next = { ...booking, ...change };
    onSea({ ...sea, booking: Object.values(next).some(Boolean) ? next : undefined });
  };
  function updateDocument(index: number, change: Partial<RouteDocument>) {
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
            {translate('route.documents.bookingNumber')}
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
            {translate('route.documents.placedDate')}
            <IsoDateInput
              value={booking.placed_date ?? ''}
              onChange={(value) => updateBooking({ placed_date: value || undefined })}
            />
          </label>
          <label>
            {translate('route.documents.confirmedDate')}
            <IsoDateInput
              value={booking.confirmed_date ?? ''}
              onChange={(value) => updateBooking({ confirmed_date: value || undefined })}
            />
          </label>
          {events.map(([key, label]) => (
            <div key={key}>
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
      {documents.map((document, index) => (
        <div className="record-fields" key={index}>
          {sea && (
            <Select
              label={translate('route.documents.role')}
              value={document.role ?? ''}
              options={[
                { value: '', label: '—' },
                { value: 'ocean', label: translate('route.documents.ocean') },
                { value: 'feeder', label: translate('route.documents.feeder') },
              ]}
              onChange={(role) =>
                updateDocument(index, {
                  role: role === 'ocean' || role === 'feeder' ? role : undefined,
                })
              }
            />
          )}
          <label>
            {translate(sea ? 'route.documents.billNumber' : 'route.documents.airNumber')}
            <input
              required
              maxLength={128}
              value={document.number}
              onChange={(event) => updateDocument(index, { number: event.target.value })}
            />
          </label>
          <label>
            {translate('route.documents.date')}
            <IsoDateInput
              value={document.date ?? ''}
              onChange={(value) => updateDocument(index, { date: value || undefined })}
            />
          </label>
          <button
            type="button"
            onClick={() => {
              const next = documents.filter((_, position) => position !== index);
              onDocuments(next.length ? next : undefined);
            }}
          >
            {translate('route.documents.remove')}
          </button>
        </div>
      ))}
      <button
        type="button"
        disabled={documents.length >= 20}
        onClick={() =>
          onDocuments([
            ...documents,
            {
              type: sea ? 'bill_of_lading' : 'air_waybill',
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
  documents?: RouteDocument[];
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
        {documents.map((document, index) => (
          <div key={index}>
            <dt>
              {translate(
                document.type === 'air_waybill'
                  ? 'route.documents.airNumber'
                  : document.role === 'ocean'
                    ? 'route.documents.ocean'
                    : document.role === 'feeder'
                      ? 'route.documents.feeder'
                      : 'route.documents.billNumber',
              )}
            </dt>
            <dd>
              {document.number}
              {document.date && ` · ${calendarDate(document.date)}`}
            </dd>
          </div>
        ))}
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
    </section>
  );
}
