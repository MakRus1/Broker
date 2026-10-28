import { filterRequest, type ColumnFilter } from './registryFilters';
import type { CalendarDate } from './CalendarDate';
export type ShipmentSort = string;
export interface ShipmentRegistryCursor {
  id: string;
  scope: string;
  text_value?: string;
  date_value?: string;
}
export interface ShipmentRegistryQuery {
  filters?: ColumnFilter[];
  number_query?: string;
  cargo_query?: string;
  customer_id?: string;
  statuses?: string[];
  date_from?: CalendarDate;
  date_to?: CalendarDate;
  sort: ShipmentSort;
  descending: boolean;
}
export const initialShipmentQuery: ShipmentRegistryQuery = {
  sort: 'shipment_date',
  descending: true,
};
export function shipmentRegistryRequest(
  query: ShipmentRegistryQuery,
  cursor?: ShipmentRegistryCursor,
) {
  return {
    ...query,
    filters: filterRequest(query.filters),
    number_query: query.number_query?.trim() || undefined,
    cargo_query: query.cargo_query?.trim() || undefined,
    limit: 50,
    ...(cursor ? { cursor } : {}),
  };
}
