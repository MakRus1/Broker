import { calendarDate } from '../../../shared/dates/calendar';
import type { Inquiry } from '../types';
import type { Rfq } from './types';
import { quotationRouteState } from './quotationRoute';
import { sharedRouteDirection } from '../../routes/SharedRouteDetails';
import { registryValueLabel } from '../../registry/registryLabels';
import { useLocalization } from '../../localization/Localization';

export function QuotationRouteNotice({ rfq, inquiry }: { rfq: Rfq; inquiry: Inquiry }) {
  const { translate, locale } = useLocalization();
  const state = quotationRouteState(rfq, inquiry.route_segments);
  if (state === 'current') return null;
  const index = inquiry.route_segments.findIndex(
    (segment) => segment.id === rfq.input.route_segment_id,
  );
  const current = inquiry.route_segments[index];
  const direction = (segment: Rfq['route_segment']) =>
    `${registryValueLabel('mode', segment.type, locale) ?? segment.type} · ${sharedRouteDirection(segment, locale)}`;
  const schedule = (segment: Rfq['route_segment']) => {
    const dates = segment.sea
      ? [
          ['ETA', segment.sea.eta],
          [translate('route.vesselDeparture'), segment.sea.departure_date],
        ]
      : segment.air
        ? [
            ['ETD', segment.air.etd],
            ['ETA', segment.air.eta],
            [translate('route.departureDate'), segment.air.departure_date],
            [translate('route.arrivalDate'), segment.air.arrival_date],
          ]
        : [
            [translate('route.departureDate'), segment.plan?.departure_date],
            [translate('route.arrivalDate'), segment.plan?.arrival_date],
          ];
    return dates
      .filter(([, value]) => value)
      .map(([label, value]) => `${label}: ${calendarDate(value)}`)
      .join(' · ');
  };
  return (
    <div className="rfq-route-notice">
      <strong>
        {translate(state === 'removed' ? 'rfq.routeRemoved' : 'rfq.routeChanged', {
          number: index + 1,
        })}
      </strong>
      <p>{translate('rfq.routeChangedHint')}</p>
      <dl>
        <div>
          <dt>{translate('rfq.quotedRoute')}</dt>
          <dd>
            {direction(rfq.route_segment)}
            <small>{schedule(rfq.route_segment)}</small>
          </dd>
        </div>
        {current && (
          <div>
            <dt>{translate('rfq.currentRoute')}</dt>
            <dd>
              {direction(current)}
              <small>{schedule(current)}</small>
            </dd>
          </div>
        )}
      </dl>
      {state === 'changed' && <small>{translate('rfq.routeDetailsChanged')}</small>}
    </div>
  );
}
