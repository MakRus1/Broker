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
type Detail = Voyage & { ports: string[]; bookings: Booking[] };

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
          {data.ports.map((port, index) => (
            <li key={index}>{port}</li>
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
