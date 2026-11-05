import { useActivationLoad } from '../../shared/api/useActivationLoad';
import { VisitedPanel } from '../../shared/ui/VisitedPanel';
import { draftStorageKey, useDraftRecovery } from '../../shared/forms/useDraftRecovery';
import { DraftRecovery } from '../../shared/ui/record/DraftRecovery';
import { RecordHeading, RecordSection } from '../../shared/ui/record/RecordLayout';
import { registryValueLabel } from '../registry/registryLabels';
import { useLocalization } from '../localization/Localization';
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
  const { translate, locale, catalogVersion } = useLocalization();
  const [transportation, setTransportation] = useState<Transportation>();
  useEffect(() => {
    if (transportation) onTitle?.(transportationTitle(transportation, locale));
  }, [transportation?.container_number, transportation?.type, locale, catalogVersion]);
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
      document.title = `${transportationTitle(transportation, locale)} · Broker`;
  }, [transportation?.container_number, transportation?.type, active, locale, catalogVersion]);
  const [failure, setFailure] = useState('');
  const [tab, setTab] = useState<'details' | 'route' | 'finance' | 'history'>(
    editing ? 'route' : 'details',
  );
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
  useActivationLoad(active, refresh, (error) => {
    setFailure(error instanceof Error ? error.message : String(error));
    onError(error);
  });
  const write = session.permissions.includes('transportations.write');
  if (!transportation)
    return (
      <section className="creation-page surface">
        <a href="#/transportations">{translate('forms.backToTransportations')}</a>
        <p>{failure || translate('forms.openingTransportation')}</p>
        <button onClick={() => void refresh().catch(onError)}>{translate('filter.retry')}</button>
      </section>
    );
  const customerName = customers.find((item) => item.id === shipment?.customer_id)?.name;
  return (
    <div className="transportation-page">
      <section className="surface record-workbench transportation-workbench">
        <RecordHeading
          guide="transportation"
          kind={translate('forms.transportation')}
          title={transportationTitle(transportation, locale)}
          status={transportation.hazardous ? translate('registryField.hazardous') : undefined}
          metadata={
            <>
              <a
                className="record-layout-reference"
                href={`#/shipments/${transportation.shipment_id}`}
              >
                {translate('registryField.shipment_id')} {shipment?.number || '↗'}
              </a>
              {' · '}
              {translate('shipment.revision', { revision: transportation.revision })}
            </>
          }
          mode={
            editing
              ? translate('transport.editingRoute')
              : editingDetails
                ? translate('transport.editingDetails')
                : undefined
          }
          actions={
            <>
              {!editing && !editingDetails && (
                <>
                  <button type="button" onClick={() => void refresh().catch(onError)}>
                    {translate('action.refresh')}
                  </button>
                  {write && (
                    <button
                      className="primary"
                      onClick={() => {
                        if (tab === 'route') location.hash = `/transportations/${id}/route`;
                        else {
                          setTab('details');
                          setEditingDetails(true);
                        }
                      }}
                    >
                      {translate('shipment.edit')}
                    </button>
                  )}
                </>
              )}
            </>
          }
        />
        {failure && (
          <p className="error" role="alert">
            {failure}
          </p>
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
            <CardNavigation className="record-layout-navigation" tabsClassName="record-layout-tabs">
              <button
                aria-pressed={tab === 'details'}
                onClick={(event) => switchTab('details', event.currentTarget)}
              >
                {translate('shipment.details')}
              </button>
              <button
                aria-pressed={tab === 'route'}
                onClick={(event) => switchTab('route', event.currentTarget)}
              >
                {translate('registryField.route')}
              </button>
              {session.permissions.includes('finance.read_service_charges') && (
                <button
                  aria-pressed={tab === 'finance'}
                  onClick={(event) => switchTab('finance', event.currentTarget)}
                >
                  {translate('home.finance')}
                </button>
              )}
              <button
                aria-pressed={tab === 'history'}
                onClick={(event) => switchTab('history', event.currentTarget)}
              >
                {translate('finance.history')}
              </button>
            </CardNavigation>
            <div className="panel-content">
              <VisitedPanel active={tab === 'details'} className="record-layout-content">
                <RecordSection title={translate('transport.details')}>
                  <dl className="record-layout-details">
                    <div>
                      <dt>{translate('registryField.type')}</dt>
                      <dd>
                        {registryValueLabel('type', transportation.type, locale) ??
                          transportationTypes[transportation.type]}
                      </dd>
                    </div>
                    <div>
                      <dt>{translate('registryValue.roles.customer')}</dt>
                      <dd>{customerName || '—'}</dd>
                    </div>
                    {transportation.type === 'container' && (
                      <>
                        <div>
                          <dt>{translate('registryField.container_number')}</dt>
                          <dd>{transportation.container_number || '—'}</dd>
                        </div>
                        <div>
                          <dt>{translate('registryField.container_type')}</dt>
                          <dd>{transportation.container_type_code || '—'}</dd>
                        </div>
                        <div>
                          <dt>{translate('forms.containerOwnership')}</dt>
                          <dd>{transportation.container_ownership || '—'}</dd>
                        </div>
                      </>
                    )}
                    <div>
                      <dt>{translate('registryField.hazardous')}</dt>
                      <dd>
                        {translate(
                          transportation.hazardous
                            ? 'transport.hazardousYes'
                            : 'transport.hazardousNo',
                        )}
                      </dd>
                    </div>
                  </dl>
                </RecordSection>
                <RecordSection
                  title={translate('transport.shipmentTerms')}
                  description={translate('transport.shipmentTermsHint')}
                >
                  <dl className="record-layout-details">
                    <div>
                      <dt>Incoterms</dt>
                      <dd>{transportation.incoterms || '—'}</dd>
                    </div>
                    <div>
                      <dt>{translate('shipment.loading')}</dt>
                      <dd>{transportation.loading || '—'}</dd>
                    </div>
                    <div>
                      <dt>{translate('shipment.carriageTerms')}</dt>
                      <dd>{transportation.carriage_terms || '—'}</dd>
                    </div>
                  </dl>
                </RecordSection>
              </VisitedPanel>
              <VisitedPanel active={tab === 'finance'}>
                <ServiceCharges
                  active={active && tab === 'finance'}
                  scope={{ shipment_id: transportation.shipment_id, transportation_id: id }}
                  session={session}
                  customers={customers}
                />
              </VisitedPanel>
              <VisitedPanel active={tab === 'history'}>
                <History
                  key={transportation.revision}
                  id={id}
                  session={session}
                  customers={customers}
                  onError={onError}
                />
              </VisitedPanel>
              <VisitedPanel active={tab === 'route'}>
                <div className="section-heading">
                  <div>
                    <h2>{translate('forms.transportationRoute')}</h2>
                    <p className="hint">
                      {translate('forms.routeLegsInTravelOrderTimeZone')}{' '}
                      {Intl.DateTimeFormat().resolvedOptions().timeZone}.
                    </p>
                  </div>
                </div>
                <RouteView
                  route_segments={transportation.route_segments}
                  customers={customers}
                  session={session}
                  onActual={
                    write ? (route_segment, kind) => setActual({ route_segment, kind }) : undefined
                  }
                />
              </VisitedPanel>
            </div>
          </>
        )}
      </section>
      {confirmation}
      {actual && (
        <Dialog
          title={`${actual.kind === 'departure' ? translate('forms.departure') : translate('forms.arrival')} · ${routePoint(actual.route_segment, 'origin').name} → ${routePoint(actual.route_segment, 'destination').name}`}
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
  const { translate, locale, catalogVersion } = useLocalization();
  const collapse = useRouteCollapse(route_segments.map((stage) => stage.id));
  if (!route_segments.length)
    return (
      <div className="empty">
        <h3>{translate('forms.noRouteYet')}</h3>
        <p>{translate('transport.inheritRouteHint')}</p>
      </div>
    );
  return (
    <div className="route-list">
      <div className="route-collapse-actions">
        <button type="button" onClick={collapse.expandAll}>
          {translate('route.expandAll')}
        </button>
        <button type="button" onClick={collapse.collapseAll}>
          {translate('route.collapseAll')}
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
              {index + 1}.{' '}
              {registryValueLabel('mode', route_segment.mode, locale) ??
                modes[route_segment.mode]}{' '}
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
            <p className="hint">{translate('transport.historicalLegHint')}</p>
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
                  {translate('forms.esr')}
                  {route_segment.rail?.origin_station?.esr_code ?? '—'} →{' '}
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
                    <strong>
                      {kind === 'departure'
                        ? translate('forms.departure')
                        : translate('forms.arrival')}
                    </strong>
                    <span>
                      {translate('forms.planned')}{' '}
                      {formatDate(
                        kind === 'departure'
                          ? route_segment.planned_departure_at
                          : route_segment.planned_arrival_at,
                        locale,
                      )}
                    </span>
                    <span>
                      {translate('forms.actual')}
                      {formatDate(value, locale)}
                    </span>
                    {onActual && route_segment.inherited !== false && (
                      <button type="button" onClick={() => onActual(route_segment, kind)}>
                        {value ? translate('forms.correct') : translate('forms.recordEvent')}
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
  const { translate, locale, catalogVersion } = useLocalization();
  const previous =
    kind === 'departure' ? route_segment.actual_departure_at : route_segment.actual_arrival_at;
  const [initialValue] = useState(() => localDateTime(previous ?? new Date().toISOString()));
  const [value, setValue] = useState(initialValue);
  const [reason, setReason] = useState('');
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const unchanged = value === localDateTime(previous);
  const storageKey = draftStorageKey(
    session,
    'transportation-actual',
    `${transportation.id}-${route_segment.id}-${kind}`,
  );
  const recovery = useDraftRecovery({
    storageKey,
    revision: transportation.revision,
    value: { value, reason },
    dirty: value !== initialValue || !!reason,
    enabled: true,
    onRestore(raw) {
      const next = raw as { value: string; reason: string };
      if (!next || typeof next.value !== 'string' || typeof next.reason !== 'string')
        throw new Error('Invalid draft');
      setValue(next.value);
      setReason(next.reason);
    },
  });
  async function submit(event: FormEvent) {
    event.preventDefault();
    if (busy || recovery.candidate) return;
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
          occurred_at: utcDateTime(value, locale),
          ...(previous ? { reason: reason.trim() } : {}),
        },
      );
      recovery.markSaved();
      onSaved(result.transportation);
    } catch (error) {
      setFailure(error instanceof Error ? error.message : translate('forms.couldNotSaveTheDate'));
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  return (
    <form
      data-recovery-key={storageKey}
      onSubmit={submit}
      data-dirty={value !== initialValue || Boolean(reason)}
      data-busy={busy}
    >
      <DraftRecovery recovery={recovery} />
      {previous && (
        <p>
          {translate('forms.current')}
          {formatDate(previous, locale)}
        </p>
      )}
      <label>
        {translate('forms.dateAndTime')}
        <DateTimeInput
          required
          disabled={busy || !!recovery.candidate}
          value={value}
          onChange={(dateValue) => setValue(dateValue)}
        />
      </label>
      <p className="hint">
        {translate('forms.timeZoneHint', {
          zone: Intl.DateTimeFormat().resolvedOptions().timeZone,
        })}
      </p>
      {previous && (
        <label>
          {translate('forms.reasonForCorrectionRequired')}
          <textarea
            required
            maxLength={2048}
            disabled={busy || !!recovery.candidate}
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
        {busy
          ? translate('shipment.saving')
          : previous
            ? translate('forms.correctDate')
            : translate('forms.recordDate')}
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
  const { translate, locale, catalogVersion } = useLocalization();
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
      setFailure(error instanceof Error ? error.message : translate('forms.couldNotLoadHistory'));
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  return (
    <section>
      <h2>{translate('forms.changeHistory')}</h2>
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      {busy && <p>{translate('filter.loading')}</p>}
      {!busy && !failure && !items.length && <p>{translate('forms.noChangesYet')}</p>}
      {items.map((item) => (
        <article className="history-entry" key={item.id}>
          <button
            className="history-row"
            onClick={() => setSelected(selected === item.id ? undefined : item.id)}
            aria-expanded={selected === item.id}
          >
            <strong>
              {item.action in historyActions
                ? translate(historyActions[item.action as keyof typeof historyActions])
                : item.action}
            </strong>
            <span>
              {item.actor_name || translate('forms.nameNotRecorded')} ·{' '}
              {formatDate(item.occurred_at, locale)}
            </span>
            <span>{translate('forms.revisionValue', { revision: item.revision })}</span>
          </button>
          {selected === item.id && (
            <div className="history-detail">
              {item.reason && (
                <p>
                  <strong>{translate('forms.reason')}</strong> {item.reason}
                </p>
              )}
              <p className="hint">{translate('forms.stateAfterChange')}</p>
              <RouteView route_segments={item.route_segments} customers={customers} />
            </div>
          )}
        </article>
      ))}
      <AutoLoadMore enabled={more} loading={busy} failed={Boolean(failure)} onLoad={next} />
    </section>
  );
}
