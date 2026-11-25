import { RouteStageTitle } from './RouteStageTitle';
import { TransportationRouteHeading } from './TransportationRouteHeading';
import { RoutePlanView } from './RoutePlan';
import { registryValueLabel } from '../registry/registryLabels';
import { useLocalization } from '../localization/Localization';
import { DateTimeInput, IsoDateInput } from '../../shared/dates/DateInput';
import { businessDate } from '../../shared/dates/calendar';
import { RoadFields } from './RoadFields';
import { SharedRouteDetailsView, sharedRouteDirection } from './SharedRouteDetails';
import {
  routePoint,
  routeDirection,
  localDateTime,
  utcDateTime,
  type PlannedRouteSegment,
} from '../transportations/transportation';
import { RailFields } from './RailFields';
import { useRouteCollapse } from './routeCollapse';
import { modes } from './logisticsLabels';
import type { Session } from '../../shared/api/session';
import type { Transportation } from '../transportations/types';
import { CustomerSelect, type Counterparty } from '../counterparties/CustomerSelect';

export function RouteEditorFields({
  initial,
  session,
  customers,
  segments,
  onChange,
  disabled = false,
  onError,
}: {
  initial: Pick<Transportation, 'shipment_id' | 'route_segments'>;
  session: Session;
  customers: Counterparty[];
  segments: PlannedRouteSegment[];
  onChange: (value: PlannedRouteSegment[]) => void;
  disabled?: boolean;
  onError: (message: string) => void;
}) {
  const { translate, locale } = useLocalization();
  const collapse = useRouteCollapse(segments.map((stage) => stage.id));
  const sharedStage = (stage: PlannedRouteSegment) =>
    initial.route_segments.find((value) => value.id === stage.id) ?? stage;
  const update = (index: number, value: PlannedRouteSegment) =>
    onChange(segments.map((stage, i) => (i === index ? value : stage)));
  const setFailure = onError;
  return (
    <div className="route-editor">
      <TransportationRouteHeading
        railHint={segments.some(({ mode }) => mode === 'rail' || mode === 'international_rail')}
        shipmentId={
          segments.some(({ mode }) => mode === 'sea' || mode === 'air')
            ? initial.shipment_id
            : undefined
        }
        onExpand={collapse.expandAll}
        onCollapse={collapse.collapseAll}
      />
      <fieldset disabled={disabled}>
        {segments.map((route_segment, index) =>
          route_segment.mode === 'sea' || route_segment.mode === 'air' ? (
            <details
              className="route-segment"
              key={route_segment.id}
              open={!collapse.collapsed.has(route_segment.id)}
              onToggle={(event) => collapse.toggle(route_segment.id, !event.currentTarget.open)}
            >
              <summary className="route-stage-summary">
                <RouteStageTitle
                  type={route_segment.mode}
                  title={
                    registryValueLabel('mode', route_segment.mode, locale) ??
                    modes[route_segment.mode]
                  }
                  index={index}
                />
                <span className="route-summary-meta">
                  {sharedRouteDirection(sharedStage(route_segment), locale)}
                </span>
              </summary>
              {route_segment.mode === 'sea' && (
                <div className="route-fields">
                  <label>
                    {translate('transport.seaUnloadingDate')}
                    <IsoDateInput
                      value={route_segment.sea_unloading_date ?? ''}
                      min="1900-01-01"
                      max={businessDate()}
                      onChange={(date) =>
                        update(index, { ...route_segment, sea_unloading_date: date || null })
                      }
                    />
                  </label>
                </div>
              )}
              <div className="shared-route-inherited">
                <SharedRouteDetailsView
                  mode={route_segment.mode}
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
                <RouteStageTitle
                  type={route_segment.mode}
                  title={
                    registryValueLabel('mode', route_segment.mode, locale) ??
                    modes[route_segment.mode]
                  }
                  index={index}
                />
                <span className="route-summary-meta">
                  <span>
                    {routeDirection(
                      { ...route_segment, plan: sharedStage(route_segment).plan },
                      locale,
                    )}
                  </span>
                  <span className="route-summary-carrier">
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
                {sharedStage(route_segment).plan && (
                  <section className="route-plan-source">
                    <h4>{translate('route.sourcePlan')}</h4>
                    <RoutePlanView value={sharedStage(route_segment).plan} />
                  </section>
                )}
                <div className="route-contractor-field">
                  <CustomerSelect
                    label={translate('registryValue.roles.carrier')}
                    items={customers}
                    session={session}
                    disabled={disabled}
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
                ) : route_segment.mode === 'rail' || route_segment.mode === 'international_rail' ? (
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
      {!segments.length && <p className="hint">{translate('forms.setTheRouteInTheShipment')}</p>}
    </div>
  );
}
