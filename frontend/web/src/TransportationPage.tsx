import { useCallback, useEffect, useState, type FormEvent } from 'react';
import {
  ApiError,
  request,
  modes,
  type Session,
  type Shipment,
  type Transportation,
  type TransportationEvent,
} from './api';
import { CardNavigation, Dialog } from './components';
import type { Counterparty } from './CustomerSelect';
import {
  formatDate,
  historyActions,
  localDateTime,
  utcDateTime,
  type RouteSegment,
} from './transportation';
import { RouteEditor } from './RouteEditor';

type Props = {
  id: string;
  editing: boolean;
  session: Session;
  customers: Counterparty[];
  onError: (error: unknown) => void;
};
export function TransportationPage({ id, editing, session, customers, onError }: Props) {
  const [transportation, setTransportation] = useState<Transportation>();
  const [shipment, setShipment] = useState<Shipment>();
  useEffect(() => {
    if (transportation) document.title = `${transportation.number} · Broker`;
  }, [transportation?.number]);
  const [failure, setFailure] = useState('');
  const [tab, setTab] = useState<'route' | 'history'>('route');
  const [actual, setActual] = useState<{
    route_segment: RouteSegment;
    kind: 'departure' | 'arrival';
  }>();
  const refresh = useCallback(
    async (signal?: AbortSignal) => {
      const result = await request<{ transportation: Transportation }>(
        '/api/v1/transportations/get',
        session,
        { transportation_id: id },
        signal,
      );
      if (signal?.aborted) return;
      setTransportation(result.transportation);
      setFailure('');
      if (session.permissions.includes('shipments.read')) {
        const parent = await request<{ shipment: Shipment }>(
          '/api/v1/shipments/get',
          session,
          { shipment_id: result.transportation.shipment_id },
          signal,
        );
        if (!signal?.aborted) setShipment(parent.shipment);
      }
    },
    [id, session],
  );
  useEffect(() => {
    const controller = new AbortController();
    void refresh(controller.signal).catch((error) => {
      if (!controller.signal.aborted) {
        setFailure(error.message);
        onError(error);
      }
    });
    return () => controller.abort();
  }, [refresh, onError]);
  const write = session.permissions.includes('transportations.write');
  if (!transportation)
    return (
      <section className="creation-page surface">
        <a href="#/transportations">← К перевозкам</a>
        <p>{failure || 'Открываем перевозку…'}</p>
        <button onClick={() => void refresh().catch(onError)}>Повторить</button>
      </section>
    );
  const hasActuals = transportation.route_segments.some(
    (route_segment) => route_segment.actual_departure_at || route_segment.actual_arrival_at,
  );
  const customerName = customers.find((item) => item.id === shipment?.customer_id)?.name;
  return (
    <div className="shipment-page">
      <section className="shipment-card">
        <div className="panel-heading">
          <div>
            <p className="eyebrow">ПЕРЕВОЗКА</p>
            <h1>{transportation.number}</h1>
          </div>
          <a href="#/transportations">← К перевозкам</a>
        </div>
        <div className="shipment-summary">
          <div>
            <span>Поставка</span>
            <a href={`#/shipments/${transportation.shipment_id}`}>
              {shipment?.number || 'Открыть поставку'} ↗
            </a>
          </div>
          <div>
            <span>Клиент</span>
            <strong>{customerName || '—'}</strong>
          </div>
          <div>
            <span>Контейнер · принадлежность</span>
            <strong>
              {transportation.container_type_code} · {transportation.container_ownership}
            </strong>
          </div>
        </div>
        {editing ? (
          <RouteEditor
            initial={transportation}
            session={session}
            customers={customers}
            allowed={write && !hasActuals}
            onSaved={(value) => {
              setTransportation(value);
              if (location.hash === `#/transportations/${id}/route`)
                location.hash = `/transportations/${id}`;
            }}
            onError={onError}
          />
        ) : (
          <>
            <CardNavigation onRefresh={() => refresh().catch(onError)}>
              <button aria-pressed={tab === 'route'} onClick={() => setTab('route')}>
                Маршрут и факты
              </button>
              <button aria-pressed={tab === 'history'} onClick={() => setTab('history')}>
                История
              </button>
            </CardNavigation>
            <div className="panel-content">
              {tab === 'history' ? (
                <History
                  key={transportation.revision}
                  id={id}
                  session={session}
                  customers={customers}
                  onError={onError}
                />
              ) : (
                <>
                  <div className="section-heading">
                    <div>
                      <h2>Маршрут перевозки</h2>
                      <p className="hint">
                        Участки маршрута в порядке следования. Время:{' '}
                        {Intl.DateTimeFormat().resolvedOptions().timeZone}.
                      </p>
                    </div>
                    {write && !hasActuals && (
                      <a className="primary" href={`#/transportations/${id}/route`}>
                        {transportation.route_segments.length
                          ? 'Редактировать маршрут'
                          : 'Добавить маршрут'}
                      </a>
                    )}
                  </div>
                  {hasActuals && (
                    <p className="route-notice">
                      Маршрут с фактическими датами нельзя заменить. Для изменения даты используйте
                      «Исправить».
                    </p>
                  )}
                  <RouteView
                    route_segments={transportation.route_segments}
                    customers={customers}
                    onActual={
                      write
                        ? (route_segment, kind) => setActual({ route_segment, kind })
                        : undefined
                    }
                  />
                </>
              )}
            </div>
          </>
        )}
      </section>
      {actual && (
        <Dialog
          title={`${actual.kind === 'departure' ? 'Отправление' : 'Прибытие'} · ${actual.route_segment.origin.name} → ${actual.route_segment.destination.name}`}
          onClose={() => setActual(undefined)}
        >
          <ActualForm
            transportation={transportation}
            route_segment={actual.route_segment}
            kind={actual.kind}
            session={session}
            onSaved={(value) => {
              setTransportation(value);
              setActual(undefined);
            }}
            onError={onError}
          />
        </Dialog>
      )}
    </div>
  );
}

export function RouteView({
  route_segments,
  customers,
  onActual,
}: {
  route_segments: RouteSegment[];
  customers: Counterparty[];
  onActual?: (route_segment: RouteSegment, kind: 'departure' | 'arrival') => void;
}) {
  if (!route_segments.length)
    return (
      <div className="empty">
        <h3>Маршрут ещё не задан</h3>
        <p>Добавьте участки и плановые даты перевозки.</p>
      </div>
    );
  return (
    <div className="route-list">
      {route_segments.map((route_segment, index) => (
        <article className="route-segment" key={route_segment.id}>
          <div className="section-heading">
            <h3>
              {index + 1}. {modes[route_segment.mode]} · {route_segment.origin.name} →{' '}
              {route_segment.destination.name}
            </h3>
          </div>
          <p className="route-segment-meta">
            {route_segment.origin.country_code} → {route_segment.destination.country_code} ·
            Подрядчик:{' '}
            {customers.find((item) => item.id === route_segment.contractor_id)?.name ||
              (route_segment.contractor_id ? 'Недоступен' : 'Не выбран')}
          </p>
          <div className="route-segment-dates">
            {(['departure', 'arrival'] as const).map((kind) => {
              const value =
                kind === 'departure'
                  ? route_segment.actual_departure_at
                  : route_segment.actual_arrival_at;
              return (
                <div key={kind}>
                  <strong>{kind === 'departure' ? 'Отправление' : 'Прибытие'}</strong>
                  <span>
                    План:{' '}
                    {formatDate(
                      kind === 'departure'
                        ? route_segment.planned_departure_at
                        : route_segment.planned_arrival_at,
                    )}
                  </span>
                  <span>Факт: {formatDate(value)}</span>
                  {onActual && (
                    <button type="button" onClick={() => onActual(route_segment, kind)}>
                      {value ? 'Исправить' : 'Записать факт'}
                    </button>
                  )}
                </div>
              );
            })}
          </div>
        </article>
      ))}
    </div>
  );
}

function ActualForm({
  transportation,
  route_segment,
  kind,
  session,
  onSaved,
  onError,
}: {
  transportation: Transportation;
  route_segment: RouteSegment;
  kind: 'departure' | 'arrival';
  session: Session;
  onSaved: (value: Transportation) => void;
  onError: (error: unknown) => void;
}) {
  const previous =
    kind === 'departure' ? route_segment.actual_departure_at : route_segment.actual_arrival_at;
  const [value, setValue] = useState(localDateTime(previous));
  const [reason, setReason] = useState('');
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const unchanged = value === localDateTime(previous);
  async function submit(event: FormEvent) {
    event.preventDefault();
    setBusy(true);
    setFailure('');
    try {
      const result = await request<{ transportation: Transportation }>(
        `/api/v1/transportation-actuals/${previous ? 'correct' : 'record'}`,
        session,
        {
          transportation_id: transportation.id,
          expected_revision: transportation.revision,
          route_segment_id: route_segment.id,
          event_kind: kind,
          occurred_at: utcDateTime(value),
          ...(previous ? { reason: reason.trim() } : {}),
        },
      );
      onSaved(result.transportation);
    } catch (error) {
      setFailure(
        error instanceof ApiError && error.status === 409
          ? 'Данные изменились или дата уже записана. Закройте форму, обновите перевозку и проверьте факт перед повтором.'
          : error instanceof Error
            ? error.message
            : 'Не удалось сохранить дату',
      );
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  return (
    <form onSubmit={submit} data-dirty={!unchanged || Boolean(reason)} data-busy={busy}>
      {previous && <p>Сейчас: {formatDate(previous)}</p>}
      <label>
        Дата и время
        <input
          type="datetime-local"
          required
          disabled={busy}
          value={value}
          onChange={(event) => setValue(event.target.value)}
        />
      </label>
      <p className="hint">
        Часовой пояс: {Intl.DateTimeFormat().resolvedOptions().timeZone}. На сервере время хранится
        в UTC.
      </p>
      {previous && (
        <label>
          Причина исправления · обязательно
          <textarea
            required
            maxLength={2048}
            disabled={busy}
            value={reason}
            onChange={(event) => setReason(event.target.value)}
          />
        </label>
      )}
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      <button
        className="primary"
        disabled={busy || !value || (Boolean(previous) && (unchanged || !reason.trim()))}
      >
        {busy ? 'Сохраняем…' : previous ? 'Исправить дату' : 'Записать дату'}
      </button>
    </form>
  );
}

function History({
  id,
  session,
  customers,
  onError,
}: {
  id: string;
  session: Session;
  customers: Counterparty[];
  onError: (error: unknown) => void;
}) {
  const [items, setItems] = useState<TransportationEvent[]>([]);
  const [busy, setBusy] = useState(true);
  const [more, setMore] = useState(false);
  const [selected, setSelected] = useState<number>();
  const [failure, setFailure] = useState('');
  useEffect(() => {
    const controller = new AbortController();
    request<{ items: TransportationEvent[] }>(
      '/api/v1/transportations/history',
      session,
      { transportation_id: id, limit: 50 },
      controller.signal,
    )
      .then((page) => {
        if (!controller.signal.aborted) {
          setItems(page.items);
          setMore(page.items.length === 50);
        }
      })
      .catch((error) => {
        if (!controller.signal.aborted) {
          setFailure(error.message);
          onError(error);
        }
      })
      .finally(() => {
        if (!controller.signal.aborted) setBusy(false);
      });
    return () => controller.abort();
  }, [id, session, onError]);
  async function next() {
    setBusy(true);
    try {
      const page = await request<{ items: TransportationEvent[] }>(
        '/api/v1/transportations/history',
        session,
        { transportation_id: id, before: items.at(-1)!.id, limit: 50 },
      );
      setItems((old) => [...old, ...page.items]);
      setMore(page.items.length === 50);
      setFailure('');
    } catch (error) {
      setFailure(error instanceof Error ? error.message : 'Не удалось загрузить историю');
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  return (
    <section>
      <h2>История изменений</h2>
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      {busy && <p>Загрузка…</p>}
      {!busy && !failure && !items.length && <p>Изменений пока нет.</p>}
      {items.map((item) => (
        <article className="history-entry" key={item.id}>
          <button
            className="history-row"
            onClick={() => setSelected(selected === item.id ? undefined : item.id)}
            aria-expanded={selected === item.id}
          >
            <strong>{historyActions[item.action] || item.action}</strong>
            <span>
              {item.actor_name || 'Имя не сохранено'} · {formatDate(item.occurred_at)}
            </span>
            <span>Редакция {item.revision}</span>
          </button>
          {selected === item.id && (
            <div className="history-detail">
              {item.reason && (
                <p>
                  <strong>Причина:</strong> {item.reason}
                </p>
              )}
              <p className="hint">Состояние после изменения</p>
              <RouteView route_segments={item.route_segments} customers={customers} />
            </div>
          )}
        </article>
      ))}
      {more && (
        <button disabled={busy} onClick={() => void next()}>
          Ранее
        </button>
      )}
    </section>
  );
}
