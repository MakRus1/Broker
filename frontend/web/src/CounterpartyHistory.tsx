import { EmployeeName } from './EmployeeName';
import { AutoLoadMore } from './AutoLoadMore';
import { useEffect, useRef, useState } from 'react';
import { request, type Session } from './api';
import { formatDate } from './transportation';
import {
  counterpartyActions,
  counterpartyChanges,
  historyFieldValue,
  type CounterpartyEvent,
} from './counterpartyChanges';

export function CounterpartyHistory({
  id,
  session,
  onError,
  version,
}: {
  id: string;
  session: Session;
  onError: (error: unknown) => void;
  version: number;
}) {
  const [items, setItems] = useState<CounterpartyEvent[]>([]);
  const [selected, setSelected] = useState<CounterpartyEvent>();
  const [more, setMore] = useState(false);
  const [busy, setBusy] = useState(true);
  const [failure, setFailure] = useState('');
  const pending = useRef<AbortController | undefined>(undefined);
  const [showUnchanged, setShowUnchanged] = useState(false);
  useEffect(() => {
    const controller = new AbortController();
    pending.current?.abort();
    pending.current = controller;
    setItems([]);
    setMore(false);
    setSelected(undefined);
    setFailure('');
    setBusy(true);
    void request<{ items: CounterpartyEvent[] }>(
      '/api/v1/counterparties/history',
      session,
      { id, limit: 50 },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) {
          setItems(result.items);
          setMore(result.items.length === 50);
        }
      })
      .catch((error) => {
        if (!controller.signal.aborted) {
          setFailure(error instanceof Error ? error.message : 'Не удалось загрузить историю');
          onError(error);
        }
      })
      .finally(() => {
        if (!controller.signal.aborted) setBusy(false);
      });
    return () => {
      controller.abort();
      pending.current?.abort();
    };
  }, [id, session, version, onError]);
  async function next() {
    if (busy || !items.length) return;
    const controller = new AbortController();
    pending.current = controller;
    setBusy(true);
    setFailure('');
    try {
      const result = await request<{ items: CounterpartyEvent[] }>(
        '/api/v1/counterparties/history',
        session,
        { id, before: items.at(-1)!.id, limit: 50 },
        controller.signal,
      );
      if (!controller.signal.aborted) {
        setItems((old) => [...old, ...result.items]);
        setMore(result.items.length === 50);
      }
    } catch (error) {
      if (!controller.signal.aborted) {
        setFailure(error instanceof Error ? error.message : 'Не удалось загрузить историю');
        onError(error);
      }
    } finally {
      if (!controller.signal.aborted) setBusy(false);
    }
  }
  if (selected) {
    const changes = counterpartyChanges(selected);
    const created = selected.action === 'create_counterparty';
    return (
      <section className="counterparty-history">
        <div className="section-heading">
          <h2>{counterpartyActions[selected.action] || selected.action}</h2>
          <button onClick={() => setSelected(undefined)}>← К истории</button>
        </div>
        <p>
          {selected.actor_name || 'Имя не сохранено'} · {formatDate(selected.occurred_at)}
          {selected.revision !== undefined && ` · Редакция ${selected.revision}`}
        </p>
        {!selected.before && !created && (
          <p className="route-notice">
            Состояние до этого изменения не сохранено. Показаны доступные данные после изменения.
          </p>
        )}
        <label className="history-unchanged">
          <input
            type="checkbox"
            checked={showUnchanged}
            onChange={(event) => setShowUnchanged(event.target.checked)}
          />
          Показать неизменённые поля
        </label>
        <div className="table-scroll">
          <table className="counterparty-diff">
            <thead>
              <tr>
                <th>Поле</th>
                <th>До изменения</th>
                <th>После изменения</th>
              </tr>
            </thead>
            <tbody>
              {changes
                .filter((change) => change.changed || showUnchanged)
                .map((change) => (
                  <tr key={change.key} className={change.changed ? 'changed' : ''}>
                    <th scope="row">{change.label}</th>
                    <td className={change.changed ? 'before' : ''}>
                      {created ? (
                        'Контрагент ещё не создан'
                      ) : change.key === 'logist' ? (
                        <EmployeeName subject={selected.before?.logist} session={session} />
                      ) : (
                        historyFieldValue(selected.before, change.key)
                      )}
                    </td>
                    <td className={change.changed ? 'after' : ''}>
                      {change.key === 'logist' ? (
                        <EmployeeName subject={selected.after.logist} session={session} />
                      ) : (
                        historyFieldValue(selected.after, change.key)
                      )}
                    </td>
                  </tr>
                ))}
            </tbody>
          </table>
        </div>
        {!changes.some((change) => change.changed) && (
          <p className="hint">Значения полей не изменились.</p>
        )}
      </section>
    );
  }
  return (
    <section className="counterparty-history" aria-busy={busy}>
      <h2>История изменений</h2>
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      {!busy && !failure && !items.length && <p className="hint">Изменений пока нет.</p>}
      {items.map((event) => (
        <article className="history-entry" key={event.id}>
          <button
            className="history-row"
            onClick={() => {
              setSelected(event);
              setShowUnchanged(false);
            }}
          >
            <strong>{counterpartyActions[event.action] || event.action}</strong>
            <span title={event.actor}>
              {event.actor_name || 'Имя не сохранено'} · {formatDate(event.occurred_at)}
            </span>
            <span>
              {event.revision === undefined
                ? 'Редакция не сохранена'
                : `Редакция ${event.revision}`}
            </span>
          </button>
        </article>
      ))}
      <AutoLoadMore enabled={more} loading={busy} failed={Boolean(failure)} onLoad={next} />
    </section>
  );
}
