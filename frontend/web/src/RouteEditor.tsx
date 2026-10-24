import { SharedRouteDetailsView, sharedRouteDirection } from './SharedRouteDetails';
import { routePoint } from './transportation';
import { RailFields } from './RailFields';
import { useRouteCollapse } from './routeCollapse';
import { useEffect, useState, type FormEvent } from 'react';
import { request, modes, type Session, type Transportation } from './api';
import { useDiscardConfirmation } from './components';
import { CustomerSelect, type Counterparty } from './CustomerSelect';
import {
  localDateTime,
  routeSegment,
  routeRequest,
  utcDateTime,
  type PlannedRouteSegment,
} from './transportation';

type Draft = { revision: number; route_segments: PlannedRouteSegment[] };
function readDraft(key: string, fallback: Draft): Draft {
  try {
    const value = JSON.parse(sessionStorage.getItem(key) || 'null');
    if (
      value &&
      Number.isSafeInteger(value.revision) &&
      value.revision > 0 &&
      Array.isArray(value.route_segments) &&
      value.route_segments.length <= 32 &&
      value.route_segments.every(
        (route_segment: PlannedRouteSegment) =>
          route_segment &&
          typeof route_segment.id === 'string' &&
          route_segment.mode in modes &&
          [route_segment.origin, route_segment.destination].every(
            (point) =>
              point === undefined ||
              (typeof point.name === 'string' && typeof point.country_code === 'string'),
          ) &&
          [
            route_segment.contractor_id,
            route_segment.planned_departure_at,
            route_segment.planned_arrival_at,
          ].every((field) => field === undefined || typeof field === 'string'),
      )
    )
      return value;
  } catch {
    /* Повреждённый локальный черновик не мешает открыть серверный маршрут. */
  }
  return fallback;
}
export function RouteEditor({
  initial,
  session,
  customers,
  allowed,
  onSaved,
  onError,
}: {
  initial: Transportation;
  session: Session;
  customers: Counterparty[];
  allowed: boolean;
  onSaved: (value: Transportation) => void;
  onError: (error: unknown) => void;
}) {
  const key = `broker:route-draft:${session.actor}:${initial.id}`;
  const [baseline] = useState<Draft>(() => ({
    revision: initial.revision,
    route_segments: initial.route_segments
      .filter((stage) => stage.inherited !== false)
      .map(routeSegment),
  }));
  const [draft, setDraft] = useState(() => readDraft(key, baseline));
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const dirty = JSON.stringify(draft.route_segments) !== JSON.stringify(baseline.route_segments);
  const collapse = useRouteCollapse(draft.route_segments.map((stage) => stage.id));
  const stale = draft.revision !== initial.revision;
  const sharedStage = (stage: PlannedRouteSegment) =>
    initial.route_segments.find((value) => value.id === stage.id) ?? stage;
  useEffect(() => {
    if (!allowed) return;
    try {
      if (dirty) sessionStorage.setItem(key, JSON.stringify(draft));
      else sessionStorage.removeItem(key);
    } catch {
      /* Хранилище может быть запрещено браузером. */
    }
  }, [key, draft, allowed, dirty]);
  useEffect(() => {
    function leaving(event: BeforeUnloadEvent) {
      if (dirty || busy) {
        event.preventDefault();
        event.returnValue = '';
      }
    }
    window.addEventListener('beforeunload', leaving);
    return () => window.removeEventListener('beforeunload', leaving);
  }, [dirty, busy]);
  function update(index: number, value: PlannedRouteSegment) {
    setDraft((old) => ({
      ...old,
      route_segments: old.route_segments.map((route_segment, i) =>
        i === index ? value : route_segment,
      ),
    }));
  }
  function discard() {
    if (busy) return;
    confirmDiscard(dirty, () => {
      try {
        sessionStorage.removeItem(key);
      } catch {
        /* Отмена доступна и без локального хранилища. */
      }
      location.hash = `/transportations/${initial.id}`;
    });
  }
  async function submit(event: FormEvent) {
    event.preventDefault();
    if (!allowed || stale) return;
    setBusy(true);
    setFailure('');
    try {
      const result = await request<{ transportation: Transportation }>(
        '/api/v1/transportation-routes/set',
        session,
        routeRequest(initial.id, draft.revision, draft.route_segments),
      );
      try {
        sessionStorage.removeItem(key);
      } catch {
        /* Результат записи не зависит от локального хранилища. */
      }
      onSaved(result.transportation);
    } catch (error) {
      setFailure(error instanceof Error ? error.message : 'Не удалось сохранить маршрут');
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  if (!allowed)
    return (
      <div className="panel-content">
        <p>Нет права на изменение перевозок.</p>
        <a href={`#/transportations/${initial.id}`}>Вернуться к перевозке</a>
      </div>
    );
  return (
    <>
      <form
        className="route-editor panel-content"
        onSubmit={submit}
        data-dirty={dirty}
        data-busy={busy}
        data-draft-key={key}
      >
        <div className="section-heading">
          <div>
            <h2>Участки маршрута</h2>
            <p className="hint">Даты ЖД-участков указываются без времени.</p>
          </div>
        </div>
        {stale && (
          <p className="error" role="alert">
            Черновик основан на редакции {draft.revision}, на сервере уже {initial.revision}.
            Отмените редактирование, чтобы открыть актуальные данные.
          </p>
        )}
        <div className="route-collapse-actions">
          <button type="button" onClick={collapse.expandAll}>
            Развернуть все
          </button>
          <button type="button" onClick={collapse.collapseAll}>
            Свернуть все
          </button>
        </div>
        <fieldset disabled={busy || stale}>
          {draft.route_segments.map((route_segment, index) =>
            route_segment.mode === 'sea' || route_segment.mode === 'air' ? (
              <details
                className="route-segment"
                key={route_segment.id}
                open={!collapse.collapsed.has(route_segment.id)}
                onToggle={(event) => collapse.toggle(route_segment.id, !event.currentTarget.open)}
              >
                <summary className="route-stage-summary">
                  <h3>
                    {index + 1}. {modes[route_segment.mode]}
                  </h3>
                  <span className="route-summary-meta">
                    {sharedRouteDirection(sharedStage(route_segment))}
                  </span>
                </summary>
                <div className="shared-route-inherited">
                  <a href={`#/shipments/${initial.shipment_id}/route`}>
                    Открыть маршрут поставки ↗
                  </a>
                  <SharedRouteDetailsView
                    sea={sharedStage(route_segment).sea}
                    air={sharedStage(route_segment).air}
                    customers={customers}
                    session={session}
                  />
                </div>
              </details>
            ) : (
              <details
                className="route-segment"
                key={route_segment.id}
                open={!collapse.collapsed.has(route_segment.id)}
                onToggle={(event) => collapse.toggle(route_segment.id, !event.currentTarget.open)}
              >
                <summary className="route-stage-summary">
                  <h3>
                    {index + 1}. {modes[route_segment.mode]}
                  </h3>
                  <span>
                    {routePoint(route_segment, 'origin').name || 'Станция не выбрана'} →{' '}
                    {routePoint(route_segment, 'destination').name || 'Станция не выбрана'}
                  </span>
                  <span className="route-summary-meta">
                    {customers.find((item) => item.id === route_segment.contractor_id)?.name ||
                      'Перевозчик не выбран'}
                    {route_segment.rail && (
                      <>
                        {' '}
                        · ETD {route_segment.rail.etd?.split('-').reverse().join('.') || '—'} · ETA{' '}
                        {route_segment.rail.eta?.split('-').reverse().join('.') || '—'}
                      </>
                    )}
                  </span>
                </summary>
                <div className="route-fields">
                  <div className="route-contractor-field">
                    <CustomerSelect
                      label={
                        route_segment.mode === 'rail' || route_segment.mode === 'international_rail'
                          ? 'Перевозчик'
                          : 'Подрядчик'
                      }
                      items={customers}
                      session={session}
                      disabled={busy}
                      value={route_segment.contractor_id ?? ''}
                      onChange={(id) =>
                        update(index, { ...route_segment, contractor_id: id || undefined })
                      }
                    />
                  </div>

                  {route_segment.mode === 'rail' || route_segment.mode === 'international_rail' ? (
                    <RailFields
                      value={route_segment.rail ?? {}}
                      session={session}
                      onChange={(rail) => {
                        const filled = Object.values(rail).some((value) => value !== undefined);
                        update(index, {
                          ...route_segment,
                          rail: filled ? rail : undefined,
                          origin: undefined,
                          destination: undefined,
                        });
                      }}
                    />
                  ) : (
                    <>
                      {(['origin', 'destination'] as const).map((point) => (
                        <div key={point} className="point-fields">
                          <label>
                            {point === 'origin' ? 'Откуда' : 'Куда'}
                            <input
                              maxLength={256}
                              value={routePoint(route_segment, point).name}
                              onChange={(event) =>
                                update(index, {
                                  ...route_segment,
                                  [point]: {
                                    ...routePoint(route_segment, point),
                                    name: event.target.value,
                                  },
                                })
                              }
                            />
                          </label>
                          <label>
                            Страна
                            <input
                              pattern="[A-Za-z]{2}"
                              minLength={2}
                              maxLength={2}
                              placeholder="RU"
                              value={routePoint(route_segment, point).country_code}
                              onChange={(event) =>
                                update(index, {
                                  ...route_segment,
                                  [point]: {
                                    ...routePoint(route_segment, point),
                                    country_code: event.target.value.toUpperCase(),
                                  },
                                })
                              }
                            />
                          </label>
                        </div>
                      ))}
                      {(['planned_departure_at', 'planned_arrival_at'] as const).map((field) => (
                        <label key={field}>
                          {field === 'planned_departure_at'
                            ? 'Плановое отправление'
                            : 'Плановое прибытие'}
                          <input
                            type="datetime-local"
                            value={localDateTime(route_segment[field])}
                            onChange={(event) => {
                              try {
                                update(index, {
                                  ...route_segment,
                                  [field]: event.target.value
                                    ? utcDateTime(event.target.value)
                                    : undefined,
                                });
                                setFailure('');
                              } catch (error) {
                                setFailure(
                                  error instanceof Error ? error.message : 'Проверьте дату',
                                );
                              }
                            }}
                          />
                        </label>
                      ))}
                    </>
                  )}
                </div>
              </details>
            ),
          )}
        </fieldset>
        {!draft.route_segments.length && <p className="hint">Задайте маршрут в поставке.</p>}
        {failure && (
          <p className="error" role="alert">
            {failure}
          </p>
        )}
        <div className="transportation-form-actions">
          <button type="button" disabled={busy} onClick={discard}>
            Отменить
          </button>
          <button
            className="primary"
            disabled={busy || stale || !dirty || !draft.route_segments.length}
          >
            {busy ? 'Сохраняем…' : 'Сохранить'}
          </button>
        </div>
      </form>
      {confirmation}
    </>
  );
}
