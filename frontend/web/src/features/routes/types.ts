export type TransportLocationKind = 'sea_port' | 'airport';
export interface TransportLocation {
  id: string;
  kind: TransportLocationKind;
  name: string;
  country_code: string;
  code?: string;
}
export interface RouteBooking {
  number?: string;
  status?: 'placed' | 'confirmed';
  placed_date?: string;
  confirmed_date?: string;
}
export interface AirWaybill {
  number: string;
  date?: string;
}
export interface BillOfLading {
  number: string;
  date?: string;
  leg?: 'ocean' | 'feeder';
}
export interface RouteDocumentEvent {
  date?: string;
}
export interface SeaDetails {
  booking?: RouteBooking;
  documents?: BillOfLading[];
  original_received?: RouteDocumentEvent;
  telex_release?: RouteDocumentEvent;
  pol?: TransportLocation;
  pod?: TransportLocation;
  eta?: string;
  departure_terminal_id?: string;
  arrival_terminal_id?: string;
  departure_date?: string;
  vessel?: string;
  voyage?: string;
}
export interface AirDetails {
  documents?: AirWaybill[];
  carrier_id?: string;
  origin_airport?: TransportLocation;
  destination_airport?: TransportLocation;
  etd?: string;
  eta?: string;
  departure_date?: string;
  arrival_date?: string;
  flight_number?: string;
}
export interface RoutePlan {
  origin_name?: string;
  origin_country?: string;
  origin_address?: string;
  destination_name?: string;
  destination_country?: string;
  destination_address?: string;
  departure_date?: string;
  arrival_date?: string;
}
export interface ShipmentRouteSegment {
  plan?: RoutePlan;
  id: string;
  type: string;
  sea?: SeaDetails;
  air?: AirDetails;
}
export interface RailwayStation {
  id: string;
  esr_code?: string;
  uic_code?: string;
  esr_source?: string;
  name: string;
  country_code: string;
}
export interface InternationalRoadDetails {
  loading_terminal_id?: string;
  planned_loading_date?: string;
  planned_unloading_date?: string;
  departure_date?: string;
  unloading_date?: string;
  vehicle_number?: string;
  trailer_number?: string;
  cmr_number?: string;
}
export interface PickupDetails {
  planned_pickup_date?: string;
  planned_delivery_date?: string;
  pickup_date?: string;
  delivery_date?: string;
  delivery_city?: string;
  delivery_country?: string;
  delivery_address?: string;
  driver?: string;
  vehicle_number?: string;
  ttn_date?: string;
}
export interface RailDetails {
  origin_station?: RailwayStation;
  destination_station?: RailwayStation;
  etd?: string;
  eta?: string;
  departure_date?: string;
  arrival_date?: string;
  unloading_date?: string;
}
