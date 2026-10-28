import { restoreColumnFilters } from './registryFilters';
import { CalendarDate } from './CalendarDate';
import { initialShipmentQuery, type ShipmentRegistryQuery } from './shipmentRegistry';
import { initialRegistryQuery, type RegistryQuery } from './transportationRegistry';
import type { CounterpartyRegistryQuery } from './counterpartyRegistry';

function object(raw: unknown): Record<string, unknown> {
  return raw && typeof raw === 'object' && !Array.isArray(raw)
    ? (raw as Record<string, unknown>)
    : {};
}
function text(raw: unknown, max = 256): string | undefined {
  return typeof raw === 'string' && raw.length <= max && raw ? raw : undefined;
}
function list(raw: unknown): string[] | undefined {
  return Array.isArray(raw) &&
    raw.length <= 100 &&
    raw.every((entry) => typeof entry === 'string' && entry.length <= 256)
    ? raw
    : undefined;
}
function choice<T extends string>(raw: unknown, allowed: readonly T[], fallback: T): T {
  return allowed.includes(raw as T) ? (raw as T) : fallback;
}
function date(raw: unknown): CalendarDate | undefined {
  try {
    return typeof raw === 'string' ? CalendarDate.fromISO(raw) : undefined;
  } catch {
    return undefined;
  }
}
export function restoreShipmentQuery(raw: unknown): ShipmentRegistryQuery {
  const value = object(raw);
  return {
    filters: restoreColumnFilters(value.filters),
    number_query: text(value.number_query),
    cargo_query: text(value.cargo_query),
    customer_id:
      typeof value.customer_id === 'string' && /^[a-f0-9-]{36}$/i.test(value.customer_id)
        ? value.customer_id
        : undefined,
    statuses: list(value.statuses),
    date_from: date(value.date_from),
    date_to: date(value.date_to),
    sort: text(value.sort, 64) ?? initialShipmentQuery.sort,
    descending:
      typeof value.descending === 'boolean' ? value.descending : initialShipmentQuery.descending,
  };
}
export function restoreTransportationQuery(raw: unknown): RegistryQuery {
  const value = object(raw);
  const types = list(value.types)?.filter(
    (entry): entry is NonNullable<RegistryQuery['types']>[number] =>
      ['consolidated', 'container', 'air', 'oversized', 'rail', 'vessel'].includes(entry),
  );
  return {
    filters: restoreColumnFilters(value.filters),
    query: text(value.query),
    types: types?.length ? types : undefined,
    container_types: list(value.container_types),
    hazardous: typeof value.hazardous === 'boolean' ? value.hazardous : undefined,
    sort: text(value.sort, 64) ?? initialRegistryQuery.sort,
    descending:
      typeof value.descending === 'boolean' ? value.descending : initialRegistryQuery.descending,
  };
}
export const initialCounterpartyQuery: CounterpartyRegistryQuery = {
  query: '',
  status: 'active',
  sort: 'name',
  descending: false,
};
export function restoreCounterpartyQuery(raw: unknown): CounterpartyRegistryQuery {
  const value = object(raw);
  return {
    filters: restoreColumnFilters(value.filters),
    query: text(value.query) ?? '',
    role:
      choice(value.role, ['customer', 'carrier', 'agent', 'supplier', 'partner'], '') || undefined,
    status: choice(value.status, ['active', 'archived', 'all'], 'active'),
    country_code:
      typeof value.country_code === 'string' && /^[A-Z]{2}$/.test(value.country_code)
        ? value.country_code
        : undefined,
    sort: text(value.sort, 64) ?? 'name',
    descending: value.descending === true,
  };
}
