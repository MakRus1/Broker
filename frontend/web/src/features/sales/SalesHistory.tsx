import { AutoLoadMore } from '../../shared/ui/AutoLoadMore';
import { useEffect, useState, type ReactNode } from 'react';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { EmployeeName } from '../../shared/profile/EmployeeName';
import { useLocalization } from '../localization/Localization';
import { CalendarDate } from '../../shared/dates/CalendarDate';
import {
  RecordHistoryDiff,
  RecordHistoryEntry,
  type HistoryDiffRow,
} from '../../shared/ui/record/RecordHistory';
import { SalesCustomerName } from './SalesCustomerName';
import { InquiryShipmentLink } from './InquiryShipmentLink';
import type { SalesEntity, SalesResource, SalesSettings } from './types';
export function SalesHistory({
  resource,
  id,
  session,
  settings,
}: {
  resource: SalesResource;
  id: string;
  session: Session;
  settings?: SalesSettings;
}) {
  const { translate } = useLocalization();
  const [items, setItems] = useState<
    { revision: number; actor: string; created_at: string; snapshot: SalesEntity }[]
  >([]);
  const [error, setError] = useState('');
  const [after, setAfter] = useState(0);
  const [attempt, setAttempt] = useState(0);
  const [more, setMore] = useState(false);
  const [loading, setLoading] = useState(false);
  useEffect(() => {
    const controller = new AbortController();
    setLoading(true);
    setError('');
    request<{ items: typeof items }>(
      `/api/v1/sales/${resource}/history`,
      session,
      { id, after_revision: after },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) {
          setItems((old) => (after === 0 ? result.items : [...old, ...result.items]));
          setMore(result.items.length === 100);
        }
      })
      .catch((failure) => {
        if (!controller.signal.aborted) setError(failure.message);
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false);
      });
    return () => controller.abort();
  }, [resource, id, session, after, attempt]);
  return (
    <section className="record-history" aria-label={translate('forms.changeHistory')}>
      <div className="section-heading">
        <h2>{translate('forms.changeHistory')}</h2>
      </div>
      {error && (
        <p className="error" role="alert">
          {error}{' '}
          <button type="button" onClick={() => setAttempt((value) => value + 1)}>
            {translate('filter.retry')}
          </button>
        </p>
      )}
      {!loading && !error && !items.length && (
        <p className="empty-state">{translate('history.empty')}</p>
      )}
      {items.map((item, index) => {
        const previous = items[index - 1];
        const before = previous?.revision === item.revision - 1 ? previous.snapshot : undefined;
        const actor = item.actor.split('#').at(-1);
        return (
          <RecordHistoryEntry
            key={item.revision}
            title={`${translate(resource === 'cards' ? 'sales.card' : 'sales.inquiry')} · ${translate(item.revision === 1 ? 'history.created' : 'history.updated')}`}
            actor={<EmployeeName subject={actor} session={session} />}
            occurredAt={item.created_at}
            revision={translate('sales.revision', { revision: item.revision })}
            initiallyOpen={index === 0}
          >
            {!before && item.revision > 1 && (
              <p className="route-notice">{translate('history.previousMissing')}</p>
            )}
            <SalesHistoryChanges
              before={before}
              after={item.snapshot}
              session={session}
              settings={settings}
            />
          </RecordHistoryEntry>
        );
      })}
      <AutoLoadMore
        enabled={more && !error}
        loading={loading}
        onLoad={async () => setAfter(items.at(-1)?.revision ?? 0)}
      />
    </section>
  );
}

function SalesHistoryChanges({
  before,
  after,
  session,
  settings,
}: {
  before?: SalesEntity;
  after: SalesEntity;
  session: Session;
  settings?: SalesSettings;
}) {
  const { translate } = useLocalization();
  const rows: HistoryDiffRow[] = [];
  function add<T>(
    key: Parameters<typeof translate>[0],
    oldValue: T | undefined,
    value: T | undefined,
    render: (value: T) => ReactNode,
  ) {
    if (oldValue === value) return;
    rows.push({
      key,
      label: translate(key),
      before: oldValue == null || oldValue === '' ? '—' : render(oldValue),
      after: value == null || value === '' ? '—' : render(value),
    });
  }
  const plain = (value: string) => value;
  const date = (value: string) => CalendarDate.fromISO(value).toDisplayString();
  add('sales.customer', before?.customer_id, after.customer_id, (id) => (
    <SalesCustomerName id={id} session={session} />
  ));
  add('sales.manager', before?.manager_subject, after.manager_subject, (subject) => (
    <EmployeeName subject={subject} session={session} />
  ));
  if ('stage_id' in after) {
    const old = before && 'stage_id' in before ? before : undefined;
    add(
      'sales.stage',
      old?.stage_id,
      after.stage_id,
      (id) => settings?.stages.find((stage) => stage.id === id)?.name ?? id,
    );
    add('sales.source', old?.source, after.source, plain);
    add('sales.nextAction', old?.next_action, after.next_action, plain);
    add('sales.nextActionDate', old?.next_action_date, after.next_action_date, date);
  } else {
    const old = before && 'status' in before ? before : undefined;
    add('registryField.number', old?.number, after.number, plain);
    add('sales.status', old?.status, after.status, (status) => translate(`sales.status.${status}`));
    add('sales.cargo', old?.cargo_description, after.cargo_description, plain);
    add('sales.readyDate', old?.ready_date, after.ready_date, date);
    add('sales.deliveryDate', old?.desired_delivery_date, after.desired_delivery_date, date);
    add('sales.incoterms', old?.incoterms, after.incoterms, plain);
    add('sales.carriage_terms', old?.carriage_terms, after.carriage_terms, plain);
    add('sales.loading', old?.loading, after.loading, plain);
    add('sales.delivery_city', old?.delivery_city, after.delivery_city, plain);
    add('sales.delivery_country', old?.delivery_country, after.delivery_country, plain);
    add('sales.delivery_address', old?.delivery_address, after.delivery_address, plain);
    add('field.shipment', old?.shipment_id, after.shipment_id, (id) => (
      <InquiryShipmentLink key={id} id={id} session={session} />
    ));
  }
  add('sales.note', before?.note, after.note, plain);
  return <RecordHistoryDiff rows={rows} />;
}
