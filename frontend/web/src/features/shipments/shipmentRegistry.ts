import { filterRequest, type ColumnFilter } from '../registry/registryFilters';
export type ShipmentSort = string;
export interface ShipmentRegistryCursor {
  id: string;
  scope: string;
  text_value?: string;
  date_value?: string;
}
export interface ShipmentRegistryQuery {
  filters?: ColumnFilter[];
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
    limit: 50,
    ...(cursor ? { cursor } : {}),
  };
}
