import type { ShipmentVoyageLink } from '../shipments/types';
import { RecordPage } from '../../shared/ui/record/RecordPage';
import { useCallback, useEffect, useState } from 'react';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { useActivationLoad } from '../../shared/api/useActivationLoad';
import { RecordHeading, RecordSection } from '../../shared/ui/record/RecordLayout';
import { useLocalization } from '../localization/Localization';

type Voyage = { id: string; number: string };
type Booking = {
  shipment_id: string;
  shipment_number: string;
  booking_number: string;
  load_port: string;
  discharge_port: string;
};
type PortCall = { name: string; position: number };
type Detail = Voyage & { ports: PortCall[]; bookings: Booking[] };

function useVoyages<T>(operation: string, session: Session, active: boolean, voyageId?: string) {
  const [data, setData] = useState<T>();
  const [failure, setFailure] = useState('');
  const load = useCallback(
    async (signal: AbortSignal) => {
      const value = await request<T>(
        '/api/v1/voyages/' + operation,
        session,
        { voyage_id: voyageId },
        signal,
      );
      if (!signal.aborted) {
        setData(value);
        setFailure('');
      }
    },
    [operation, session, voyageId],
  );
  useActivationLoad(active, load, (error) => setFailure(String(error)));
  return { data, failure };
}

export type VoyageChange = {
  segment_id: string;
  voyage_id?: string;
  loading_call?: number;
  discharge_call?: number;
};

function BookingCalls({
  session,
  change,
  onChange,
}: {
  session: Session;
  change: VoyageChange;
  onChange: (change: VoyageChange) => void;
}) {
  const { translate } = useLocalization();
  const { data, failure } = useVoyages<Detail>('get', session, true, change.voyage_id);
  const choices = (calls: PortCall[]) =>
    calls.map((call) => (
      <option key={call.position} value={call.position}>
        {call.position}. {call.name}
      </option>
    ));
  return (
    <>
      <label>
        {translate('voyage.load')}
        <select
          required
          value={change.loading_call ?? ''}
          onChange={(e) =>
            onChange({
              ...change,
              loading_call: Number(e.target.value) || undefined,
              discharge_call: undefined,
            })
          }
        >
          <option value="">{translate('voyage.chooseCall')}</option>
          {choices(data?.ports.slice(0, -1) ?? [])}
        </select>
      </label>
      <label>
        {translate('voyage.discharge')}
        <select
          required
          value={change.discharge_call ?? ''}
          onChange={(e) =>
            onChange({ ...change, discharge_call: Number(e.target.value) || undefined })
          }
        >
          <option value="">{translate('voyage.chooseCall')}</option>
          {choices(
            data?.ports.filter(
              (call) => change.loading_call !== undefined && call.position > change.loading_call,
            ) ?? [],
          )}
        </select>
      </label>
      {failure && <p role="alert">{failure}</p>}
    </>
  );
}

export function BookingVoyage({
  session,
  segmentId,
  current,
  change,
  onChange,
}: {
  session: Session;
  segmentId: string;
  current?: ShipmentVoyageLink;
  change?: VoyageChange;
  onChange: (change?: VoyageChange) => void;
}) {
  const { translate } = useLocalization();
  const [editing, setEditing] = useState(false);
  const selecting = editing || change !== undefined;
  const { data, failure } = useVoyages<{ items: Voyage[] }>('list', session, selecting);
  return (
    <div className="booking-voyage-fields">
      {selecting ? (
        <>
          <label>
            {translate('voyage.title')}
            <select
              value={change?.voyage_id ?? (current && !change ? '__current' : '')}
              onChange={(e) =>
                onChange({ segment_id: segmentId, voyage_id: e.target.value || undefined })
              }
            >
              {current && !change && (
                <option value="__current">
                  {current.voyage_number} · {current.load_port} → {current.discharge_port}
                </option>
              )}
              <option value="">{translate('voyage.external')}</option>
              {data?.items.map((voyage) => (
                <option key={voyage.id} value={voyage.id}>
                  {voyage.number}
                </option>
              ))}
            </select>
          </label>
          {change?.voyage_id && (
            <BookingCalls
              key={change.voyage_id}
              session={session}
              change={change}
              onChange={onChange}
            />
          )}
          {failure && <p role="alert">{failure}</p>}
          <button
            type="button"
            onClick={() => {
              onChange(undefined);
              setEditing(false);
            }}
          >
            {translate('action.cancel')}
          </button>
        </>
      ) : (
        <>
          {current && (
            <span>
              {current.voyage_number} · {current.load_port} → {current.discharge_port}
            </span>
          )}
          <button
            type="button"
            onClick={() => {
              setEditing(true);
            }}
          >
            {translate(current ? 'voyage.changeAssignment' : 'voyage.assign')}
          </button>
        </>
      )}
    </div>
  );
}

type Props = { session: Session; active: boolean };
export function VoyageRegistry({ session, active }: Props) {
  const { translate } = useLocalization();
  const { data, failure } = useVoyages<{ items: Voyage[] }>('list', session, active);
  return (
    <section className="surface">
      <h1>{translate('nav.voyages')}</h1>
      {failure ? (
        <p role="alert">{failure}</p>
      ) : !data ? (
        <p>{translate('common.loading')}</p>
      ) : data.items.length ? (
        <ul>
          {data.items.map((voyage) => (
            <li key={voyage.id}>
              <a href={'#/voyages/' + voyage.id}>{voyage.number}</a>
            </li>
          ))}
        </ul>
      ) : (
        <p>{translate('voyage.noVoyages')}</p>
      )}
    </section>
  );
}

export function VoyageCard({
  id,
  session,
  active,
  onTitle,
}: Props & { id: string; onTitle: (id: string, title: string) => void }) {
  const { translate } = useLocalization();
  const { data, failure } = useVoyages<Detail>('get', session, active, id);
  useEffect(() => {
    if (data) onTitle('/voyages/' + id, data.number);
  }, [id, data, onTitle]);
  if (failure) return <p role="alert">{failure}</p>;
  if (!data) return <p>{translate('common.loading')}</p>;
  return (
    <RecordPage>
      <RecordHeading kind={translate('voyage.title')} title={data.number} />
      <RecordSection title={translate('voyage.calls')}>
        <ol>
          {data.ports.map((port) => (
            <li key={port.position}>{port.name}</li>
          ))}
        </ol>
      </RecordSection>
      <RecordSection title={translate('voyage.bookings')}>
        {data.bookings.length ? (
          <div className="route-document-table-scroll">
            <table className="route-document-table">
              <thead>
                <tr>
                  <th>{translate('field.shipment')}</th>
                  <th>{translate('voyage.booking')}</th>
                  <th>{translate('voyage.load')}</th>
                  <th>{translate('voyage.discharge')}</th>
                </tr>
              </thead>
              <tbody>
                {data.bookings.map((b, i) => (
                  <tr key={i}>
                    <td>
                      <a href={'#/shipments/' + b.shipment_id}>{b.shipment_number}</a>
                    </td>
                    <td>{b.booking_number || '—'}</td>
                    <td>{b.load_port}</td>
                    <td>{b.discharge_port}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        ) : (
          <p>{translate('voyage.noBookings')}</p>
        )}
      </RecordSection>
    </RecordPage>
  );
}
