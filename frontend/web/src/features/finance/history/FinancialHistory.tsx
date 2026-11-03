import { useLocalization } from '../../localization/Localization';
import { EmployeeName } from '../../../shared/profile/EmployeeName';
import { useEffect, useRef, useState } from 'react';
import { request } from '../../../shared/api/http';
import type { Session } from '../../../shared/api/session';
import { AutoLoadMore } from '../../../shared/ui/AutoLoadMore';
import {
  historyValue,
  historyTitle,
  historyFieldLabel,
  historyChangeKey,
  historyReferenceIdsDiffer,
  type HistoryEvent,
  type HistoryKind,
  type HistoryPage,
} from './historyTypes';
import './financialHistory.css';

export function FinancialHistory({
  kind,
  id,
  revision,
  session,
}: {
  kind: HistoryKind;
  id: string;
  revision: number;
  session: Session;
}) {
  const { translate } = useLocalization();
  const [page, setPage] = useState<HistoryPage>({ items: [] });
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const [version, setVersion] = useState(0);
  const controller = useRef<AbortController | undefined>(undefined);
  useEffect(() => {
    const pending = new AbortController();
    controller.current = pending;
    setPage({ items: [] });
    setBusy(true);
    setFailure('');
    void request<HistoryPage>(`/api/v1/${kind}/history`, session, { id, limit: 30 }, pending.signal)
      .then((value) => {
        if (!pending.signal.aborted) setPage(value);
      })
      .catch((error) => {
        if (!pending.signal.aborted) setFailure(error.message);
      })
      .finally(() => {
        if (!pending.signal.aborted) setBusy(false);
      });
    return () => controller.current?.abort();
  }, [kind, id, revision, session, version]);
  async function more() {
    if (busy || !page.next_before) return;
    const pending = new AbortController();
    controller.current = pending;
    setBusy(true);
    setFailure('');
    try {
      const next = await request<HistoryPage>(
        `/api/v1/${kind}/history`,
        session,
        { id, limit: 30, before: page.next_before },
        pending.signal,
      );
      if (!pending.signal.aborted)
        setPage((old) => ({ ...next, items: [...old.items, ...next.items] }));
    } catch (error) {
      if (!pending.signal.aborted)
        setFailure(error instanceof Error ? error.message : translate('forms.couldNotLoadHistory'));
    } finally {
      if (!pending.signal.aborted) setBusy(false);
    }
  }
  return (
    <section className="financial-history" aria-label={translate('forms.changeHistory')}>
      <div className="section-heading">
        <h2>{translate('forms.changeHistory')}</h2>
        <button type="button" disabled={busy} onClick={() => setVersion((old) => old + 1)}>
          {translate('action.refresh')}
        </button>
      </div>
      {failure && (
        <p className="error" role="alert">
          {failure}{' '}
          <button
            type="button"
            onClick={() => (page.next_before ? void more() : setVersion((old) => old + 1))}
          >
            {translate('filter.retry')}
          </button>
        </p>
      )}
      {page.items.map((event, index) => (
        <HistoryRecord
          key={`${event.cursor.source}:${event.cursor.entity_id}:${event.cursor.revision}`}
          event={event}
          initiallyOpen={index === 0}
          session={session}
        />
      ))}
      {!busy && !failure && !page.items.length && (
        <p className="empty-state">{translate('history.empty')}</p>
      )}
      {busy && (
        <p className="hint" role="status">
          {translate('history.loading')}
        </p>
      )}
      <AutoLoadMore enabled={!!page.next_before} loading={busy} failed={!!failure} onLoad={more} />
    </section>
  );
}
function HistoryRecord({
  event,
  initiallyOpen,
  session,
}: {
  event: HistoryEvent;
  initiallyOpen: boolean;
  session: Session;
}) {
  const { translate, locale } = useLocalization();
  return (
    <details className="financial-history-event" open={initiallyOpen || undefined}>
      <summary>
        <span className="financial-history-title">{historyTitle(event, locale)}</span>
        <span
          className="financial-history-actor"
          title={event.actor_name ? undefined : translate('history.actorFallback')}
        >
          {event.actor_name || (
            <EmployeeName
              subject={event.actor.split('#').at(-1)}
              session={session}
              fallback={event.actor.split('#').at(-1) || translate('forms.nameNotRecorded')}
            />
          )}
        </span>
        <time dateTime={event.cursor.occurred_at}>
          {new Date(event.cursor.occurred_at).toLocaleString(locale)}
        </time>
        <span className="hint">
          {translate(
            event.cursor.source === 'service_charge'
              ? 'history.chargeRevision'
              : event.cursor.source === 'invoice'
                ? 'history.invoiceRevision'
                : 'history.paymentRevision',
            { revision: event.cursor.revision },
          )}
        </span>
      </summary>
      {!event.has_previous && event.cursor.revision > 1 && (
        <p className="route-notice">{translate('history.previousMissing')}</p>
      )}
      {event.changes.length ? (
        <div className="table-scroll">
          <table className="financial-history-diff">
            <thead>
              <tr>
                <th>{translate('counterparty.field')}</th>
                <th>{translate('counterparty.before')}</th>
                <th>{translate('counterparty.after')}</th>
              </tr>
            </thead>
            <tbody>
              {event.changes.map((change) => (
                <tr key={historyChangeKey(change)}>
                  <th scope="row">{historyFieldLabel(change, event.document, locale)}</th>
                  <td>
                    {historyValue(
                      change.before,
                      locale,
                      change.field,
                      historyReferenceIdsDiffer(change),
                    )}
                  </td>
                  <td>
                    {historyValue(
                      change.after,
                      locale,
                      change.field,
                      historyReferenceIdsDiffer(change),
                    )}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      ) : (
        <p className="hint">{translate('counterparty.noFieldValuesChanged')}</p>
      )}
    </details>
  );
}
