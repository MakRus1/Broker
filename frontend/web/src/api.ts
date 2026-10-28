export interface Session {
  actor: string;
  subject: string;
  display_name: string;
  csrf_token: string;
  permissions: string[];
}
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
}
export interface Operation {
  id: string;
  item_id: string;
  quantity: number;
  status: 'pending' | 'succeeded' | 'rejected';
  transportation_ids?: string[];
  rejection_code?: string;
}
export type TransportLocationKind = 'sea_port' | 'airport';
export interface TransportLocation {
  id: string;
  kind: TransportLocationKind;
  name: string;
  country_code: string;
  code?: string;
}
export interface SeaDetails {
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
  carrier_id?: string;
  origin_airport?: TransportLocation;
  destination_airport?: TransportLocation;
  etd?: string;
  eta?: string;
  departure_date?: string;
  arrival_date?: string;
  flight_number?: string;
}
export interface ShipmentRouteSegment {
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
export class ApiError extends Error {
  constructor(
    public status: number,
    message: string,
    public traceId?: string,
  ) {
    super(message);
  }
}
export async function request<T>(
  path: string,
  session?: Session,
  body?: unknown,
  signal?: AbortSignal,
): Promise<T> {
  const response = await fetch(path, {
    method: body === undefined ? 'GET' : 'POST',
    credentials: 'same-origin',
    headers:
      body === undefined
        ? {}
        : {
            'Content-Type': 'application/json',
            'X-CSRF-Token': session?.csrf_token ?? '',
          },
    body: body === undefined ? undefined : JSON.stringify(body),
    signal,
  });
  const value = await response.json().catch(() => null);
  if (!response.ok) {
    throw new ApiError(
      response.status,
      value?.message ?? 'Не удалось выполнить запрос. Попробуйте ещё раз.',
      response.headers.get('X-Trace-Id') ?? undefined,
    );
  }
  return value as T;
}
// Один ключ сохраняется для повторной отправки той же команды после сетевой ошибки.
export class Commands {
  private keys = new Map<string, { key: string; body: object }>();
  async send<T>(path: string, session: Session, body: object): Promise<T> {
    const { expected_revision: _revision, ...intent } = body as Record<string, unknown>;
    const fingerprint = path + JSON.stringify(intent);
    const command = this.keys.get(fingerprint) ?? { key: crypto.randomUUID(), body };
    this.keys.set(fingerprint, command);
    try {
      const result = await request<T>(path, session, {
        ...command.body,
        idempotency_key: command.key,
      });
      this.keys.delete(fingerprint);
      return result;
    } catch (value) {
      if (value instanceof ApiError && value.status >= 400 && value.status < 500)
        this.keys.delete(fingerprint);
      throw value;
    }
  }
}
export const statusName = {
  pending: 'Создаются',
  succeeded: 'Созданы',
  rejected: 'Отклонено',
} as const;
export const modes = {
  sea: 'Море',
  international_rail: 'Международное ЖД',
  international_road: 'Международное авто',
  rail: 'ЖД РФ',
  road: 'Вывоз',
  pickup: 'Вывоз',
  air: 'Авиа',
} as const;
export const transportationTypes = {
  consolidated: 'Сборка',
  container: 'Контейнер',
  air: 'Авиа',
  oversized: 'Негабарит',
  rail: 'ЖД',
  vessel: 'Судно',
} as const;
export const ownership = { COC: 'COC', SOC: 'SOC' } as const;
export function operationStorageKey(subject: string, shipment: string) {
  return `broker.operations:${subject}:${shipment}`;
}
export function readOperationIds(key: string): string[] {
  try {
    const value: unknown = JSON.parse(localStorage.getItem(key) ?? '[]');
    return Array.isArray(value)
      ? value
          .filter((id): id is string => typeof id === 'string' && /^[\da-f-]{36}$/i.test(id))
          .slice(-100)
      : [];
  } catch {
    return [];
  }
}
