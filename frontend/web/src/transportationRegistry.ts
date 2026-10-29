import { filterRequest, type ColumnFilter } from './registryFilters';
export type RegistrySort = string;
export interface RegistryCursor {
  id: string;
  value: string;
  scope: string;
}
export interface RouteFilterGroup {
  filters: ColumnFilter[];
}
export interface RegistryQuery {
  route_filters?: RouteFilterGroup[];
  filters?: ColumnFilter[];
  sort: RegistrySort;
  descending: boolean;
}
export const initialRegistryQuery: RegistryQuery = { sort: 'container_number', descending: false };
export function registryRequest(query: RegistryQuery, cursor?: RegistryCursor) {
  return {
    ...query,
    filters: filterRequest(query.filters),
    route_filters: query.route_filters?.map((group) => ({ filters: filterRequest(group.filters) })),
    limit: 50,
    ...(cursor ? { cursor } : {}),
  };
}
