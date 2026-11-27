import type { ShipmentRouteSegment } from '../routes/types';
import { localDate } from '../../shared/dates/calendar';
import type { Shipment } from './types';

export type CardDraft = {
  number: string;
  shipment_date: string;
  customer_id: string;
  recipient_id: string;
  logist: string;
  cargo_description: string;
  status: string;
  incoterms: string;
  carriage_terms: string;
  loading: string;
  delivery_city: string;
  delivery_country: string;
  delivery_address: string;
  reference: string;
  route_segments: ShipmentRouteSegment[];
};

export function draftOf(initial?: Shipment): CardDraft {
  return {
    number: initial?.number ?? '',
    shipment_date: initial?.shipment_date ?? localDate(),
    customer_id: initial?.customer_id ?? '',
    recipient_id: initial?.recipient_id ?? '',
    logist: initial?.logist ?? '',
    cargo_description: initial?.cargo_description ?? '',
    status: initial?.status ?? '',
    incoterms: initial?.incoterms ?? '',
    carriage_terms: initial?.carriage_terms ?? '',
    loading: initial?.loading ?? '',
    delivery_city: initial?.delivery_city ?? '',
    delivery_country: initial?.delivery_country ?? '',
    delivery_address: initial?.delivery_address ?? '',
    reference: initial?.reference ?? '',
    route_segments: initial?.route_segments ?? [],
  };
}

export function createPayload(draft: CardDraft, manualNumber: boolean) {
  return {
    ...draft,
    number: manualNumber ? draft.number.trim() : undefined,
    recipient_id: draft.recipient_id || null,
    logist: draft.logist.trim() || null,
    cargo_description: draft.cargo_description.trim() || null,
    incoterms: draft.incoterms || null,
    carriage_terms: draft.carriage_terms || null,
    loading: draft.loading || null,
    delivery_city: draft.delivery_city.trim() || null,
    delivery_country: draft.delivery_country || null,
    delivery_address: draft.delivery_address.trim() || null,
  };
}

export function updatePayload(
  draft: CardDraft,
  baseline: CardDraft,
  originalLogist: Shipment['logist'],
  manualNumber: boolean,
) {
  const payload = createPayload(draft, manualNumber);
  return {
    ...payload,
    logist: draft.logist === (originalLogist ?? '') ? undefined : payload.logist,
    route_segments:
      JSON.stringify(draft.route_segments) === JSON.stringify(baseline.route_segments)
        ? undefined
        : draft.route_segments,
  };
}
