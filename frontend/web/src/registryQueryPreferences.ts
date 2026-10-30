import { restoreColumnFilters, type ColumnFilter } from './registryFilters';
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
function restoreRouteFilters(raw: unknown) {
  if (!Array.isArray(raw) || raw.length > 8) return undefined;
  const groups = raw.map((value) => ({ filters: restoreColumnFilters(value?.filters) }));
  if (groups.some((value) => !value.filters?.length)) return undefined;
  return groups as import('./transportationRegistry').RouteFilterGroup[];
}
export function restoreTransportationQuery(raw: unknown): RegistryQuery {
  const value = object(raw);
  return {
    route_filters: restoreRouteFilters(value.route_filters),
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

export const initialContractQuery = {
  filters: [{ field: 'archived', values: ['false'] }] as ColumnFilter[],
  sort: 'contract_date',
  descending: true,
};
export function restoreContractQuery(raw: unknown): typeof initialContractQuery {
  const value = object(raw);
  return {
    filters: restoreColumnFilters(value.filters) ?? initialContractQuery.filters,
    sort: text(value.sort, 64) ?? initialContractQuery.sort,
    descending:
      typeof value.descending === 'boolean' ? value.descending : initialContractQuery.descending,
  };
}
