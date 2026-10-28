import { filterRequest, type ColumnFilter } from './registryFilters';
export type CounterpartySort = string;
export interface CounterpartyRegistryCursor {
  id: string;
  scope: string;
  value: string;
}
export interface CounterpartyRegistryQuery {
  filters?: ColumnFilter[];
  query: string;
  role?: string;
  status: 'active' | 'archived' | 'all';
  country_code?: string;
  sort: CounterpartySort;
  descending: boolean;
}
