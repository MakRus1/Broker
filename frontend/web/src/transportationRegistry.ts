import type { Transportation } from './api';
export type RegistrySort = 'id' | 'container_number' | 'container_type' | 'type';
export interface RegistryCursor {
  id: string;
  value: string;
  scope: string;
}
export interface RegistryQuery {
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
    query: query.query?.trim() || undefined,
    limit: 50,
    ...(cursor ? { cursor } : {}),
  };
}
