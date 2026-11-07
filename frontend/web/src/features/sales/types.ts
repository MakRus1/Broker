import type { ShipmentRouteSegment } from '../routes/types';
export type SalesStage = { id: string; name: string; enabled: boolean };
export type SalesSettings = { revision: number; stages: SalesStage[] };
export type SalesCard = {
  id: string;
  customer_id: string;
  manager_subject: string;
  stage_id: string;
  source?: string;
  note?: string;
  next_action?: string;
  next_action_date?: string;
  revision: number;
};
export type InquiryStatus = 'draft' | 'in_progress' | 'agreed' | 'rejected';
export type Inquiry = {
  id: string;
  customer_id: string;
  manager_subject: string;
  status: InquiryStatus;
  number: string;
  revision: number;
  conversion_error?: string;
  shipment_id?: string;
  converting: boolean;
  cargo_description?: string;
  incoterms?: string;
  carriage_terms?: string;
  loading?: string;
  delivery_city?: string;
  delivery_country?: string;
  delivery_address?: string;
  ready_date?: string;
  desired_delivery_date?: string;
  note?: string;
  route_segments: ShipmentRouteSegment[];
};
export type SalesEntity = SalesCard | Inquiry;
export type SalesResource = 'cards' | 'inquiries';
export const statuses: InquiryStatus[] = ['draft', 'in_progress', 'agreed', 'rejected'];
export function saveSalesBody(value: SalesEntity) {
  const { revision, ...body } = value;
  if ('number' in body) {
    const {
      number: _number,
      conversion_error: _conversionError,
      shipment_id: _shipment,
      converting: _converting,
      ...fields
    } = body;
    return { ...fields, expected_revision: revision };
  }
  return { ...body, expected_revision: revision };
}
