import { restoreColumnFilters } from './registryFilters';
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
export function restoreShipmentQuery(raw: unknown): ShipmentRegistryQuery {
  const value = object(raw);
  return {
    filters: restoreColumnFilters(value.filters),
    sort: text(value.sort, 64) ?? initialShipmentQuery.sort,
    descending:
      typeof value.descending === 'boolean' ? value.descending : initialShipmentQuery.descending,
  };
}
export function restoreTransportationQuery(raw: unknown): RegistryQuery {
  const value = object(raw);
  return {
    filters: restoreColumnFilters(value.filters),
    sort: text(value.sort, 64) ?? initialRegistryQuery.sort,
    descending:
      typeof value.descending === 'boolean' ? value.descending : initialRegistryQuery.descending,
  };
}
export const initialCounterpartyQuery: CounterpartyRegistryQuery = {
  filters: [{ field: 'archived', values: ['false'] }],
  sort: 'name',
  descending: false,
};
export function restoreCounterpartyQuery(raw: unknown): CounterpartyRegistryQuery {
  const value = object(raw);
  return {
    filters: restoreColumnFilters(value.filters) ?? initialCounterpartyQuery.filters,
    sort: text(value.sort, 64) ?? 'name',
    descending: value.descending === true,
  };
}
