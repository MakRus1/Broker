import { ServiceCharges } from '../finance/FinancePanel';
import { DateTimeInput } from '../../shared/dates/DateInput';
import { RoadDetailsView } from '../routes/RoadFields';
import { SharedRouteDetailsView } from '../routes/SharedRouteDetails';
import { routePoint, routeDirection } from './transportation';
import { RailDetailsView } from '../routes/RailFields';
import { calendarDate } from '../../shared/dates/calendar';
import { useRouteCollapse } from '../routes/routeCollapse';
import { AutoLoadMore } from '../../shared/ui/AutoLoadMore';
import { useCallback, useEffect, useState, type FormEvent } from 'react';
import { request } from '../../shared/api/http';
import { modes, transportationTypes } from '../routes/logisticsLabels';
import { type Session } from '../../shared/api/session';
import { type Shipment } from '../shipments/types';
import { type Transportation, type TransportationEvent } from './types';
import { CardNavigation } from '../../shared/ui/CardNavigation';
import { Dialog, useDiscardConfirmation } from '../../shared/ui/Dialog';
import type { Counterparty } from '../counterparties/CustomerSelect';
import {
  routeSegment,
  formatDate,
  transportationTitle,
  historyActions,
  localDateTime,
  utcDateTime,
  type RouteSegment,
} from './transportation';
import { RouteEditor } from '../routes/RouteEditor';
import { TransportationDetailsForm } from './TransportationDetailsForm';

type Props = {
  id: string;
  onTitle?: (title: string) => void;
  onChanged?: (value: Transportation) => void;
  active?: boolean;
  editing: boolean;
  session: Session;
  customers: Counterparty[];
  onError: (error: unknown) => void;
};
export function TransportationPage({
  id,
  onTitle,
  onChanged,
  active = true,
  editing,
  session,
  customers: suppliedCustomers,
  onError,
}: Props) {
  const [transportation, setTransportation] = useState<Transportation>();
  useEffect(() => {
    if (transportation) onTitle?.(transportationTitle(transportation));
  }, [transportation?.container_number, transportation?.type]);
  const [shipment, setShipment] = useState<Shipment>();
  const [linkedCustomers, setLinkedCustomers] = useState<Counterparty[]>([]);
  const customers = [
    ...suppliedCustomers,
    ...linkedCustomers.filter((item) => !suppliedCustomers.some((value) => value.id === item.id)),
  ];
  useEffect(() => {
    if (!session.permissions.includes('counterparties.read')) return;
    const ids = [
      ...new Set([
        ...(shipment ? [shipment.customer_id] : []),
        ...(transportation?.route_segments.flatMap((item) =>
          [item.contractor_id, item.international_road?.loading_terminal_id].filter(
            (id): id is string => Boolean(id),
          ),
        ) ?? []),
      ]),
    ].filter((id) => !suppliedCustomers.some((item) => item.id === id));
    const controller = new AbortController();
    void Promise.allSettled(
      ids.map((id) =>
        request<{ counterparty: Counterparty }>(
          '/api/v1/counterparties/get',
          session,
          { id },
          controller.signal,
        ),
      ),
    ).then((results) => {
      if (!controller.signal.aborted)
        setLinkedCustomers(
          results.flatMap((result) =>
            result.status === 'fulfilled' ? [result.value.counterparty] : [],
          ),
        );
    });
    return () => controller.abort();
  }, [session, shipment, transportation, suppliedCustomers]);
  useEffect(() => {
    if (transportation && active)
      document.title = `${transportationTitle(transportation)} · Broker`;
  }, [transportation?.container_number, transportation?.type, active]);
  const [failure, setFailure] = useState('');
  const [tab, setTab] = useState<'route' | 'finance' | 'history'>('route');
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  function switchTab(next: typeof tab, button: HTMLElement) {
    const section = button.closest('section');
    if (section?.querySelector('form[data-busy="true"]')) return;
    confirmDiscard(Boolean(section?.querySelector('form[data-dirty="true"]')), () => setTab(next));
  }
  const [editingDetails, setEditingDetails] = useState(false);
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
    if (!active) return;
    const controller = new AbortController();
    void refresh(controller.signal).catch((error) => {
      if (!controller.signal.aborted) {
        setFailure(error.message);
        onError(error);
      }
    });
    return () => controller.abort();
  }, [refresh, onError, active]);
  const write = session.permissions.includes('transportations.write');
  if (!transportation)
    return (
      <section className="creation-page surface">
        <a href="#/transportations">← К перевозкам</a>
        <p>{failure || 'Открываем перевозку…'}</p>
        <button onClick={() => void refresh().catch(onError)}>Повторить</button>
      </section>
    );
  const customerName = customers.find((item) => item.id === shipment?.customer_id)?.name;
  return (
    <div className="shipment-page">
      <section className="shipment-card">
        <div className="panel-heading">
          <div>
            <p className="eyebrow">ПЕРЕВОЗКА</p>
            <h1>{transportationTitle(transportation)}</h1>
            {(editing || editingDetails) && (
              <a
                className="transportation-parent"
                href={`#/shipments/${transportation.shipment_id}`}
              >
                Поставка {shipment?.number || '↗'}
              </a>
            )}
          </div>
          <div className="transportation-heading-actions">
            <a href="#/transportations">← К перевозкам</a>
            {write && !editing && !editingDetails && (
              <button onClick={() => setEditingDetails(true)}>Редактировать</button>
            )}
          </div>
        </div>
        {!editing && !editingDetails && (
          <>
            <div className="shipment-summary transportation-summary">
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
                <span>Тип перевозки</span>
                <strong>{transportationTypes[transportation.type]}</strong>
                {transportation.type === 'container' && (
                  <small className="transportation-container">
                    {transportation.container_type_code || 'Тип не указан'} ·{' '}
                    {transportation.container_ownership || 'Принадлежность не указана'}
                  </small>
                )}
                {transportation.hazardous && (
                  <small className="transportation-hazard">Опасный груз</small>
                )}
              </div>
            </div>
            <div className="shipment-summary transportation-summary transportation-terms">
              <div>
                <span>Incoterms</span>
                <strong>{transportation.incoterms || '—'}</strong>
              </div>
              <div>
                <span>Загрузка</span>
                <strong>{transportation.loading || '—'}</strong>
              </div>
              <div>
                <span>Условия перевозки</span>
                <strong>{transportation.carriage_terms || '—'}</strong>
              </div>
            </div>
          </>
        )}
        {editingDetails && (
          <div className="panel-content transportation-edit-content">
            <TransportationDetailsForm
              initial={transportation}
              shipmentId={transportation.shipment_id}
              session={session}
              onSaved={(value) => {
                setTransportation(value);
                onChanged?.(value);
                setEditingDetails(false);
              }}
              onCancel={() => setEditingDetails(false)}
              onError={onError}
            />
          </div>
        )}
        {editing ? (
          <RouteEditor
            initial={transportation}
            session={session}
            customers={customers}
            allowed={write}
            onSaved={(value) => {
              setTransportation(value);
              onChanged?.(value);
              if (location.hash === `#/transportations/${id}/route`)
                location.hash = `/transportations/${id}`;
            }}
            onError={onError}
          />
        ) : editingDetails ? null : (
          <>
            <CardNavigation
              onRefresh={tab === 'finance' ? undefined : () => refresh().catch(onError)}
            >
              <button
                aria-pressed={tab === 'route'}
                onClick={(event) => switchTab('route', event.currentTarget)}
              >
                Маршрут и факты
              </button>
              {session.permissions.includes('finance.read_service_charges') && (
                <button
                  aria-pressed={tab === 'finance'}
                  onClick={(event) => switchTab('finance', event.currentTarget)}
                >
                  Финансы
                </button>
              )}
              <button
                aria-pressed={tab === 'history'}
                onClick={(event) => switchTab('history', event.currentTarget)}
              >
                История
              </button>
            </CardNavigation>
            <div className="panel-content">
              {tab === 'finance' ? (
                <ServiceCharges
                  scope={{ shipment_id: transportation.shipment_id, transportation_id: id }}
                  session={session}
                  customers={customers}
                />
              ) : tab === 'history' ? (
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
                    {write && transportation.route_segments.length > 0 && (
                      <a className="primary" href={`#/transportations/${id}/route`}>
                        Заполнить участки
                      </a>
                    )}
                  </div>
                  <RouteView
                    route_segments={transportation.route_segments}
                    customers={customers}
                    session={session}
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
      {confirmation}
      {actual && (
        <Dialog
          title={`${actual.kind === 'departure' ? 'Отправление' : 'Прибытие'} · ${routePoint(actual.route_segment, 'origin').name} → ${routePoint(actual.route_segment, 'destination').name}`}
          onClose={() => setActual(undefined)}
        >
          <ActualForm
            transportation={transportation}
            route_segment={actual.route_segment}
            kind={actual.kind}
            session={session}
            onSaved={(value) => {
              setTransportation(value);
              onChanged?.(value);
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
  session,
}: {
  route_segments: RouteSegment[];
  session?: Session;
  customers: Counterparty[];
  onActual?: (route_segment: RouteSegment, kind: 'departure' | 'arrival') => void;
}) {
  const collapse = useRouteCollapse(route_segments.map((stage) => stage.id));
  if (!route_segments.length)
    return (
      <div className="empty">
        <h3>Маршрут ещё не задан</h3>
        <p>Задайте участки маршрута в поставке. Они появятся здесь автоматически.</p>
      </div>
    );
  return (
    <div className="route-list">
      <div className="route-collapse-actions">
        <button type="button" onClick={collapse.expandAll}>
          Развернуть все
        </button>
        <button type="button" onClick={collapse.collapseAll}>
          Свернуть все
        </button>
      </div>
      {route_segments.map((route_segment, index) => (
        <details
          className="route-segment"
          key={route_segment.id}
          open={!collapse.collapsed.has(route_segment.id)}
          onToggle={(event) => collapse.toggle(route_segment.id, !event.currentTarget.open)}
        >
          <summary className="route-stage-summary">
            <h3>
              {index + 1}. {modes[route_segment.mode]} · {routeDirection(route_segment)}
            </h3>
            <span className="route-summary-meta">
              {customers.find((item) => item.id === route_segment.contractor_id)?.name ||
                'Перевозчик не выбран'}
              {route_segment.rail && (
                <>
                  {' '}
                  · ETD {calendarDate(route_segment.rail.etd)} · ETA{' '}
                  {calendarDate(route_segment.rail.eta)}
                </>
              )}
            </span>
          </summary>
          {(routePoint(route_segment, 'origin').country_code ||
            routePoint(route_segment, 'destination').country_code) && (
            <p className="route-segment-meta">
              {[
                routePoint(route_segment, 'origin').country_code,
                routePoint(route_segment, 'destination').country_code,
              ]
                .filter(Boolean)
                .join(' → ')}
            </p>
          )}
          {route_segment.inherited === false && (
            <p className="hint">
              Исторический участок: отсутствует в текущем маршруте поставки. Доступен только
              просмотр.
            </p>
          )}
          {route_segment.mode === 'sea' || route_segment.mode === 'air' ? (
            <SharedRouteDetailsView
              sea={route_segment.sea}
              air={route_segment.air}
              customers={customers}
              session={session}
            />
          ) : route_segment.mode === 'international_road' ||
            route_segment.mode === 'road' ||
            route_segment.mode === 'pickup' ? (
            <RoadDetailsView
              road={route_segment.international_road}
              pickup={route_segment.pickup}
              customers={customers}
            />
          ) : route_segment.mode === 'rail' || route_segment.mode === 'international_rail' ? (
            <>
              {(route_segment.rail?.origin_station || route_segment.rail?.destination_station) && (
                <p className="rail-stations-meta">
                  ЕСР: {route_segment.rail?.origin_station?.esr_code ?? '—'} →{' '}
                  {route_segment.rail?.destination_station?.esr_code ?? '—'}
                </p>
              )}
              <RailDetailsView value={routeSegment(route_segment).rail ?? {}} />
            </>
          ) : (
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
                    {onActual && route_segment.inherited !== false && (
                      <button type="button" onClick={() => onActual(route_segment, kind)}>
                        {value ? 'Исправить' : 'Записать факт'}
                      </button>
                    )}
                  </div>
                );
              })}
            </div>
          )}
        </details>
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
  const [initialValue] = useState(() => localDateTime(previous ?? new Date().toISOString()));
  const [value, setValue] = useState(initialValue);
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
      setFailure(error instanceof Error ? error.message : 'Не удалось сохранить дату');
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  return (
    <form onSubmit={submit} data-dirty={value !== initialValue || Boolean(reason)} data-busy={busy}>
      {previous && <p>Сейчас: {formatDate(previous)}</p>}
      <label>
        Дата и время
        <DateTimeInput
          required
          disabled={busy}
          value={value}
          onChange={(dateValue) => setValue(dateValue)}
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
      <AutoLoadMore enabled={more} loading={busy} failed={Boolean(failure)} onLoad={next} />
    </section>
  );
}
