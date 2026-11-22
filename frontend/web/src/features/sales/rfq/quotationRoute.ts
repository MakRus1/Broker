import type { ShipmentRouteSegment } from '../../routes/types';
import type { Rfq } from './types';

// Object key order and omitted optional fields must not make a route appear changed.
function routeSnapshot(value: unknown): string {
  return JSON.stringify(value, (_key, item) => {
    if (item && typeof item === 'object' && !Array.isArray(item))
      return Object.fromEntries(
        Object.entries(item)
          .filter(([, field]) => field != null)
          .sort(([a], [b]) => a.localeCompare(b)),
      );
    return item;
  });
}

export function quotationRouteState(rfq: Rfq, segments: ShipmentRouteSegment[]) {
  const current = segments.find((segment) => segment.id === rfq.input.route_segment_id);
  if (!current) return 'removed';
  return routeSnapshot(current) === routeSnapshot(rfq.route_segment) ? 'current' : 'changed';
}
