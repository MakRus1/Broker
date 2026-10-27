export type CounterpartySort = 'id' | 'name' | 'country_code' | 'tax_id' | 'archived';
export interface CounterpartyRegistryCursor {
  id: string;
  scope: string;
  value: string;
}
export interface CounterpartyRegistryQuery {
  query: string;
  role?: string;
  status: 'active' | 'archived' | 'all';
  country_code?: string;
  sort: CounterpartySort;
  descending: boolean;
}
