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
export interface Transportation {
  number: string;
  id: string;
  revision: number;
  shipment_id: string;
  item_id: string;
  operation_id: string;
  container_type_code: string;
  container_ownership: 'SOC' | 'COC';
  legs: {
    id: string;
    mode: 'sea' | 'road' | 'rail';
    contractor_id?: string;
    planned_departure_at?: string;
    planned_arrival_at?: string;
    actual_departure_at?: string;
    actual_arrival_at?: string;
    origin: { name: string; country_code: string };
    destination: { name: string; country_code: string };
  }[];
}
export interface TransportationEvent {
  id: number;
  action: string;
  actor: string;
  actor_name?: string;
  occurred_at: string;
  reason?: string;
  leg_id?: string;
  event_kind?: 'departure' | 'arrival';
  revision: number;
  legs: Transportation['legs'];
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
export const modes = { sea: 'Море', road: 'Авто', rail: 'ЖД' } as const;
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
