import { filterRequest, type ColumnFilter } from './registryFilters';
import type { Transportation } from './api';
export type RegistrySort = string;
export interface RegistryCursor {
  id: string;
  value: string;
  scope: string;
}
export interface RegistryQuery {
  filters?: ColumnFilter[];
  query?: string;
  types?: Transportation['type'][];
  container_types?: string[];
  hazardous?: boolean;
  sort: RegistrySort;
  descending: boolean;
}
export const initialRegistryQuery: RegistryQuery = { sort: 'container_number', descending: false };
export function registryRequest(query: RegistryQuery, cursor?: RegistryCursor) {
  return {
    ...query,
    filters: filterRequest(query.filters),
    query: query.query?.trim() || undefined,
    limit: 50,
    ...(cursor ? { cursor } : {}),
  };
}
