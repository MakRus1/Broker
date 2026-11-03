import { draftStorageKey, useDraftRecovery } from '../../shared/forms/useDraftRecovery';
import { DraftRecovery } from '../../shared/ui/record/DraftRecovery';
import { RecordActions } from '../../shared/ui/record/RecordLayout';
import { registryValueLabel } from '../registry/registryLabels';
import { useLocalization } from '../localization/Localization';
import { DateTimeInput } from '../../shared/dates/DateInput';
import { RoadFields } from './RoadFields';
import { SharedRouteDetailsView, sharedRouteDirection } from './SharedRouteDetails';
import { routePoint, routeDirection } from '../transportations/transportation';
import { RailFields } from './RailFields';
import { useRouteCollapse } from './routeCollapse';
import { useEffect, useState, type FormEvent } from 'react';
import { request } from '../../shared/api/http';
import { modes } from './logisticsLabels';
import { type Session } from '../../shared/api/session';
import { type Transportation } from '../transportations/types';
import { useDiscardConfirmation } from '../../shared/ui/Dialog';
import { CustomerSelect, type Counterparty } from '../counterparties/CustomerSelect';
import {
  localDateTime,
  routeSegment,
  routeRequest,
  utcDateTime,
  type PlannedRouteSegment,
} from '../transportations/transportation';

type Draft = { revision: number; route_segments: PlannedRouteSegment[] };
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
  const { translate, locale } = useLocalization();
  const key = draftStorageKey(session, 'transportation-route', initial.id);
  const [baseline] = useState<Draft>(() => ({
    revision: initial.revision,
    route_segments: initial.route_segments
      .filter((stage) => stage.inherited !== false)
      .map(routeSegment),
  }));
  const [draft, setDraft] = useState(baseline);
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const dirty = JSON.stringify(draft.route_segments) !== JSON.stringify(baseline.route_segments);
  const collapse = useRouteCollapse(draft.route_segments.map((stage) => stage.id));
  const stale = draft.revision !== initial.revision;
  const sharedStage = (stage: PlannedRouteSegment) =>
    initial.route_segments.find((value) => value.id === stage.id) ?? stage;
  const recovery = useDraftRecovery({
    storageKey: key,
    revision: initial.revision,
    value: draft,
    dirty,
    enabled: allowed,
    onRestore(raw) {
      const next = raw as Draft;
      if (
        !next ||
        !Array.isArray(next.route_segments) ||
        next.route_segments.length !== baseline.route_segments.length ||
        next.route_segments.some(
          (stage, index) =>
            stage.id !== baseline.route_segments[index].id ||
            stage.mode !== baseline.route_segments[index].mode,
        )
      )
        throw new Error('Invalid draft');
      setDraft(next);
    },
  });
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
      recovery.discard();
      location.hash = `/transportations/${initial.id}`;
    });
  }
  async function submit(event: FormEvent) {
    event.preventDefault();
    if (!allowed || stale || recovery.candidate) return;
    setBusy(true);
    setFailure('');
    try {
      const result = await request<{ transportation: Transportation }>(
        '/api/v1/transportation-routes/set',
        session,
        routeRequest(initial.id, draft.revision, draft.route_segments, locale),
      );
      recovery.markSaved();
      onSaved(result.transportation);
    } catch (error) {
      setFailure(error instanceof Error ? error.message : translate('forms.couldNotSaveTheRoute'));
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  if (!allowed)
    return (
      <div className="panel-content">
        <p>{translate('forms.youDoNotHavePermissionToEditTransportations')}</p>
        <a href={`#/transportations/${initial.id}`}>{translate('forms.backToTransportation')}</a>
      </div>
    );
  return (
    <>
      <DraftRecovery recovery={recovery} />
      <form
        className="route-editor panel-content"
        onSubmit={submit}
        data-dirty={dirty}
        data-busy={busy}
        data-recovery-key={key}
      >
        <div className="section-heading">
          <div>
            <h2>{translate('forms.routeLegs')}</h2>
            <p className="hint">{translate('forms.railLegDatesDoNotIncludeATime')}</p>
          </div>
        </div>
        {stale && (
          <p className="error" role="alert">
            {translate('forms.routeDraftConflict', {
              draft: draft.revision,
              current: initial.revision,
            })}
          </p>
        )}
        <div className="route-collapse-actions">
          <button type="button" onClick={collapse.expandAll}>
            {translate('route.expandAll')}
          </button>
          <button type="button" onClick={collapse.collapseAll}>
            {translate('route.collapseAll')}
          </button>
        </div>
        <fieldset disabled={busy || stale || !!recovery.candidate}>
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
                    {index + 1}.{' '}
                    {registryValueLabel('mode', route_segment.mode, locale) ??
                      modes[route_segment.mode]}
                  </h3>
                  <span className="route-summary-meta">
                    {sharedRouteDirection(sharedStage(route_segment), locale)}
                  </span>
                </summary>
                <div className="shared-route-inherited">
                  <a href={`#/shipments/${initial.shipment_id}/route`}>
                    {translate('forms.openShipmentRoute')}
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
                    {index + 1}.{' '}
                    {registryValueLabel('mode', route_segment.mode, locale) ??
                      modes[route_segment.mode]}
                  </h3>
                  <span className="route-summary-meta">
                    <span>{routeDirection(route_segment, locale)}</span>
                    <span>
                      {customers.find((item) => item.id === route_segment.contractor_id)?.name ||
                        translate('forms.carrierNotSelected')}
                    </span>
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
                      label={translate('registryValue.roles.carrier')}
                      items={customers}
                      session={session}
                      disabled={busy}
                      value={route_segment.contractor_id ?? ''}
                      onChange={(id) =>
                        update(index, { ...route_segment, contractor_id: id || undefined })
                      }
                    />
                  </div>

                  {route_segment.mode === 'international_road' ||
                  route_segment.mode === 'road' ||
                  route_segment.mode === 'pickup' ? (
                    <RoadFields
                      international={route_segment.mode === 'international_road'}
                      road={route_segment.international_road}
                      pickup={route_segment.pickup}
                      session={session}
                      customers={customers}
                      onRoad={(international_road) =>
                        update(index, {
                          ...route_segment,
                          international_road: Object.values(international_road).some(
                            (value) => value !== undefined,
                          )
                            ? international_road
                            : undefined,
                        })
                      }
                      onPickup={(pickup) => update(index, { ...route_segment, pickup })}
                    />
                  ) : route_segment.mode === 'rail' ||
                    route_segment.mode === 'international_rail' ? (
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
                            {point === 'origin'
                              ? translate('forms.origin')
                              : translate('forms.destination')}
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
                            {translate('registryField.country_code')}
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
                            ? translate('forms.plannedDeparture')
                            : translate('forms.plannedArrival')}
                          <DateTimeInput
                            value={localDateTime(route_segment[field])}
                            onChange={(dateValue) => {
                              try {
                                update(index, {
                                  ...route_segment,
                                  [field]: dateValue ? utcDateTime(dateValue, locale) : undefined,
                                });
                                setFailure('');
                              } catch (error) {
                                setFailure(
                                  error instanceof Error
                                    ? error.message
                                    : translate('forms.checkTheDate'),
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
        {!draft.route_segments.length && (
          <p className="hint">{translate('forms.setTheRouteInTheShipment')}</p>
        )}
        <RecordActions
          error={failure}
          status={
            dirty
              ? translate(
                  recovery.persisted && !recovery.failed ? 'draft.local' : 'shipment.unsaved',
                )
              : translate('shipment.revision', { revision: initial.revision })
          }
        >
          <button type="button" disabled={busy} onClick={discard}>
            {translate('action.cancel')}
          </button>
          <button
            className="primary"
            disabled={
              busy || !!recovery.candidate || stale || !dirty || !draft.route_segments.length
            }
          >
            {busy ? translate('shipment.saving') : translate('action.save')}
          </button>
        </RecordActions>
      </form>
      {confirmation}
    </>
  );
}
