import { filterRequest, type ColumnFilter } from './registryFilters';
export type CounterpartySort = string;
export interface CounterpartyRegistryCursor {
  id: string;
  scope: string;
  value: string;
}
export interface CounterpartyRegistryQuery {
  filters?: ColumnFilter[];
  sort: CounterpartySort;
  descending: boolean;
}
