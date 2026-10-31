import type { ShipmentRouteSegment } from '../routes/types';
export interface Shipment {
  id: string;
  revision: number;
  number: string;
  customer_id: string;
  reference?: string;
  shipment_date?: string;
  recipient_id?: string | null;
  logist?: string | null;
  cargo_description?: string | null;
  status?: string;
  incoterms?: string | null;
  carriage_terms?: string | null;
  loading?: string | null;
  delivery_city?: string | null;
  delivery_country?: string | null;
  delivery_address?: string | null;
  route_segments?: ShipmentRouteSegment[];
}
export interface Item {
  id: string;
  revision: number;
  shipment_id: string;
  container_type_code: string;
  container_ownership: 'SOC' | 'COC';
  planned_quantity: number;
  created_quantity: number;
  pending_quantity: number;
  available_quantity: number;
  excess_quantity: number;
}
export interface ContainerFact {
  container_type_code?: string;
  container_ownership?: 'SOC' | 'COC';
  quantity: number;
}
export interface Operation {
  id: string;
  item_id: string;
  quantity: number;
  status: 'pending' | 'succeeded' | 'rejected';
  transportation_ids?: string[];
  rejection_code?: string;
}
