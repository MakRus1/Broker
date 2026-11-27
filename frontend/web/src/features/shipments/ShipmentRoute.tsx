import type { ShipmentVoyageLink } from './types';
import { BookingVoyage, type VoyageChange } from '../voyages/Voyages';
import '../routes/shipmentRoute.css';
import { RouteStageTitle } from '../routes/RouteStageTitle';
import { calendarDate } from '../../shared/dates/calendar';
import { RoutePlanFields, RoutePlanView } from '../routes/RoutePlan';
import { registryValueLabel } from '../registry/registryLabels';
import { useLocalization } from '../localization/Localization';
import { useRouteCollapse } from '../routes/routeCollapse';
import { Select } from '../../shared/ui/Select';
import {
  SharedRouteFields,
  SharedRouteDetailsView,
  sharedRouteDirection,
} from '../routes/SharedRouteDetails';
import type { Session } from '../../shared/api/session';
import type { ShipmentRouteSegment } from '../routes/types';
import type { Counterparty } from '../counterparties/CustomerSelect';

export type ShipmentRouteProgress = {
  total: number;
  current: { route_segment_id: string; quantity: number }[];
};

type RouteType = { code: string; name: string };
export function ShipmentRoute({
  segments,
  confirmedVoyages,
  planningOnly = false,
  progress,
  showHeading = true,
  onChange,
  onEdit,
  assignmentChanges,
  onAssignmentChange,
  allowedTypes,
  types,
  session,
  customers,
}: {
  assignmentChanges?: VoyageChange[];
  onAssignmentChange?: (segmentId: string, change?: VoyageChange) => void;
  confirmedVoyages?: ShipmentVoyageLink[];
  planningOnly?: boolean;
  progress?: ShipmentRouteProgress | null;
  showHeading?: boolean;
  segments: ShipmentRouteSegment[];
  onEdit?: () => void;
  onChange?: (segments: ShipmentRouteSegment[]) => void;
  allowedTypes: string[];
  types: RouteType[];
  session: Session;
  customers: Counterparty[];
}) {
  const { translate, locale } = useLocalization();
  const links = new Map(confirmedVoyages?.map((link) => [link.segment_id, link]));
  const collapse = useRouteCollapse(segments.map((stage) => stage.id));
  const counts = new Map(
    progress?.current.map(({ route_segment_id, quantity }) => [route_segment_id, quantity]),
  );
  const unassigned =
    (progress?.total ?? 0) -
    segments.reduce((total, segment) => total + (counts.get(segment.id) ?? 0), 0);
  const title = (type: string) =>
    registryValueLabel('mode', type, locale) ??
    types.find((item) => item.code === type)?.name ??
    type;
  const update = (index: number, value: ShipmentRouteSegment) =>
    onChange?.(segments.map((old, i) => (i === index ? value : old)));
  function move(index: number, delta: number) {
    const next = [...segments];
    [next[index], next[index + delta]] = [next[index + delta], next[index]];
    onChange?.(next);
  }
  return (
    <section className="shipment-route-workspace">
      <div className="shipment-route-heading">
        {showHeading && (
          <div>
            <h2>{translate('shipment.routeTitle')}</h2>
            <p>{translate('shipment.sharedRouteHint')}</p>
          </div>
        )}
        <div className="route-heading-actions">
          {onEdit && (
            <button className="shipment-route-edit" type="button" onClick={onEdit}>
              <svg
                width="16"
                height="16"
                viewBox="0 0 24 24"
                fill="none"
                stroke="currentColor"
                strokeWidth="1.6"
                aria-hidden="true"
              >
                <path d="m16 3 5 5-13 13H3v-5z M13 6l5 5" />
              </svg>
              {translate('shipment.editRoute')}
            </button>
          )}
          {!!segments.length && (
            <div className="route-collapse-actions">
              <button type="button" onClick={collapse.expandAll}>
                {translate('route.expandAll')}
              </button>
              <button type="button" onClick={collapse.collapseAll}>
                {translate('route.collapseAll')}
              </button>
            </div>
          )}
        </div>
      </div>
      <div className="shipment-route-stages">
        {segments.map((segment, index) => (
          <details
            className="route-segment shipment-route-stage"
            key={segment.id}
            open={!collapse.collapsed.has(segment.id)}
            onToggle={(event) => collapse.toggle(segment.id, !event.currentTarget.open)}
            onInvalidCapture={(event) => {
              event.currentTarget.open = true;
              collapse.toggle(segment.id, false);
            }}
          >
            <summary className="route-stage-summary">
              <RouteStageTitle type={segment.type} title={title(segment.type)} index={index} />
              <span className="route-summary-meta route-summary-direction">
                {sharedRouteDirection(segment, locale)}
                {!!counts.get(segment.id) && (
                  <span className="route-current-label">
                    {translate('shipment.routeInTransit', { count: counts.get(segment.id) ?? 0 })}
                  </span>
                )}
              </span>
              <span className="route-summary-dates">
                {segment.plan?.departure_date || segment.plan?.arrival_date
                  ? [
                      calendarDate(segment.plan?.departure_date),
                      calendarDate(segment.plan?.arrival_date),
                    ].join(' — ')
                  : segment.air?.etd || segment.air?.eta
                    ? [calendarDate(segment.air?.etd), calendarDate(segment.air?.eta)].join(' — ')
                    : segment.sea?.eta
                      ? `ETA ${calendarDate(segment.sea.eta)}`
                      : ''}
              </span>
            </summary>
            {onChange ? (
              <>
                <div className="shipment-route-stage-toolbar">
                  <Select
                    label={translate('registryField.mode')}
                    value={segment.type}
                    options={[...new Set([...allowedTypes, segment.type])].map((value) => ({
                      value,
                      label: title(value),
                    }))}
                    onChange={(type) => update(index, { id: segment.id, type })}
                  />
                  <div
                    className="shipment-route-actions"
                    role="group"
                    aria-label={translate('route.segmentActions', { number: index + 1 })}
                  >
                    <button
                      type="button"
                      disabled={index === 0}
                      aria-label={translate('route.moveSegmentUp', { number: index + 1 })}
                      onClick={() => move(index, -1)}
                    >
                      ↑
                    </button>
                    <button
                      type="button"
                      disabled={index === segments.length - 1}
                      aria-label={translate('route.moveSegmentDown', { number: index + 1 })}
                      onClick={() => move(index, 1)}
                    >
                      ↓
                    </button>
                    <button
                      type="button"
                      onClick={() => onChange(segments.filter((_, i) => i !== index))}
                    >
                      {translate('shipment.remove')}
                    </button>
                  </div>
                </div>
                {segment.type === 'sea' || segment.type === 'air' ? (
                  <SharedRouteFields
                    bookingAssignment={
                      segment.type === 'sea' &&
                      segment.sea?.booking?.status === 'confirmed' &&
                      session.permissions.includes('shipments.read_voyages') &&
                      onAssignmentChange && (
                        <BookingVoyage
                          session={session}
                          segmentId={segment.id}
                          current={links.get(segment.id)}
                          change={assignmentChanges?.find(
                            (change) => change.segment_id === segment.id,
                          )}
                          onChange={(change) => onAssignmentChange(segment.id, change)}
                        />
                      )
                    }
                    planningOnly={planningOnly}
                    type={segment.type}
                    sea={segment.sea}
                    air={segment.air}
                    session={session}
                    customers={customers}
                    onSea={(sea) =>
                      update(index, {
                        ...segment,
                        sea: Object.values(sea).some((value) => value !== undefined)
                          ? sea
                          : undefined,
                      })
                    }
                    onAir={(air) =>
                      update(index, {
                        ...segment,
                        air: Object.values(air).some((value) => value !== undefined)
                          ? air
                          : undefined,
                      })
                    }
                  />
                ) : (
                  <RoutePlanFields
                    value={segment.plan ?? {}}
                    session={session}
                    onChange={(plan) =>
                      update(index, {
                        ...segment,
                        plan: Object.values(plan).some((value) => value !== undefined)
                          ? plan
                          : undefined,
                      })
                    }
                  />
                )}
              </>
            ) : segment.type === 'sea' || segment.type === 'air' ? (
              <SharedRouteDetailsView
                confirmedVoyage={links.get(segment.id)}
                mode={segment.type}
                planningOnly={planningOnly}
                sea={segment.sea}
                air={segment.air}
                customers={customers}
                session={session}
              />
            ) : (
              <RoutePlanView value={segment.plan} />
            )}
          </details>
        ))}
      </div>
      {!segments.length && <p className="muted">{translate('shipment.addSegmentsHint')}</p>}
      {unassigned > 0 && (
        <p className="hint">
          {translate('shipment.routeWithoutCurrentLeg', { count: unassigned })}
        </p>
      )}
      {progress === null && (
        <p className="hint">{translate('shipment.routeProgressUnavailable')}</p>
      )}
      {onChange && (
        <button
          className="shipment-route-add"
          type="button"
          disabled={!allowedTypes.length || segments.length >= 30}
          onClick={() => {
            const id = crypto.randomUUID();
            onChange([...segments, { id, type: allowedTypes[0] }]);
            collapse.toggle(id, false);
          }}
        >
          {translate('shipment.addSegment')}
        </button>
      )}
    </section>
  );
}
