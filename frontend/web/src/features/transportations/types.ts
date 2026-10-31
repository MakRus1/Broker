import { modes } from '../routes/logisticsLabels';
import type {
  RailDetails,
  InternationalRoadDetails,
  PickupDetails,
  SeaDetails,
  AirDetails,
} from '../routes/types';
export interface Transportation {
  container_number?: string;
  id: string;
  revision: number;
  shipment_id: string;
  item_id?: string;
  operation_id?: string;
  container_type_code?: string;
  container_ownership?: 'SOC' | 'COC';
  type: 'consolidated' | 'container' | 'air' | 'oversized' | 'rail' | 'vessel';
  hazardous: boolean;
  incoterms?: string;
  loading?: string;
  carriage_terms?: string;
  route_segments: {
    id: string;
    mode: keyof typeof modes;
    inherited?: boolean;
    rail?: RailDetails;
    international_road?: InternationalRoadDetails;
    pickup?: PickupDetails;
    sea?: SeaDetails;
    air?: AirDetails;
    contractor_id?: string;
    planned_departure_at?: string;
    planned_arrival_at?: string;
    actual_departure_at?: string;
    actual_arrival_at?: string;
    origin?: { name: string; country_code: string };
    destination?: { name: string; country_code: string };
  }[];
}
export interface TransportationEvent {
  id: number;
  action: string;
  actor: string;
  actor_name?: string;
  occurred_at: string;
  reason?: string;
  route_segment_id?: string;
  event_kind?: 'departure' | 'arrival';
  revision: number;
  route_segments: Transportation['route_segments'];
}
