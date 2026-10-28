import { CalendarDate } from './CalendarDate';
export interface ColumnFilter {
  field: string;
  values?: string[];
  query?: string;
  date_from?: CalendarDate;
  date_to?: CalendarDate;
  empty?: boolean;
  labels?: Record<string, string>;
}
export interface RegistryField {
  id: string;
  label: string;
  kind: 'values' | 'set' | 'date' | 'boolean';
  sortable: boolean;
  lookup?: string;
  case_insensitive?: boolean;
}
export interface RegistryValue {
  value: string;
  label: string;
}
export function filterRequest(filters?: ColumnFilter[]) {
  return filters?.map(({ labels: _labels, ...filter }) => filter);
}
export function restoreColumnFilters(raw: unknown): ColumnFilter[] | undefined {
  if (!Array.isArray(raw) || raw.length > 32) return undefined;
  const used = new Set<string>();
  const result: ColumnFilter[] = [];
  for (const value of raw) {
    if (
      !value ||
      typeof value !== 'object' ||
      typeof value.field !== 'string' ||
      !/^[a-z_]{1,64}$/.test(value.field) ||
      used.has(value.field)
    )
      return undefined;
    used.add(value.field);
    const filter: ColumnFilter = { field: value.field };
    if (value.values !== undefined) {
      if (
        !Array.isArray(value.values) ||
        value.values.length > 200 ||
        value.values.some((x: unknown) => typeof x !== 'string' || x.length > 2000)
      )
        return undefined;
      filter.values = [...new Set<string>(value.values)];
    }
    if (typeof value.query === 'string' && value.query.length <= 256) filter.query = value.query;
    if (typeof value.empty === 'boolean') filter.empty = value.empty;
    try {
      if (value.date_from) filter.date_from = CalendarDate.fromISO(value.date_from);
      if (value.date_to) filter.date_to = CalendarDate.fromISO(value.date_to);
    } catch {
      return undefined;
    }
    if (value.labels && typeof value.labels === 'object')
      filter.labels = Object.fromEntries(
        Object.entries(value.labels).filter(
          ([key, label]) =>
            filter.values?.includes(key) && typeof label === 'string' && label.length <= 2000,
        ),
      ) as Record<string, string>;
    result.push(filter);
  }
  return result;
}
export function splitFilterValues(text: string): string[] {
  return [
    ...new Set(
      text
        .split(/[\n\r\t;,]+/)
        .map((value) => value.trim())
        .filter(Boolean),
    ),
  ];
}
export function replaceFilters(current: ColumnFilter[], fields: string[], next: ColumnFilter[]) {
  return [...current.filter((filter) => !fields.includes(filter.field)), ...next];
}
export function legacyColumnFilters(
  registry: string,
  query: Record<string, unknown>,
): ColumnFilter[] {
  const result: ColumnFilter[] = [];
  const text = (field: string, key: string) => {
    if (typeof query[key] === 'string' && query[key])
      result.push({ field, query: query[key] as string });
  };
  const values = (field: string, key: string) => {
    if (Array.isArray(query[key]) && (query[key] as string[]).length)
      result.push({ field, values: query[key] as string[] });
  };
  if (registry === 'shipments') {
    text('number', 'number_query');
    text('cargo_description', 'cargo_query');
    values('status', 'statuses');
    if (query.customer_id)
      result.push({ field: 'customer_id', values: [String(query.customer_id)] });
    if (query.date_from || query.date_to)
      result.push({
        field: 'shipment_date',
        date_from: query.date_from as CalendarDate | undefined,
        date_to: query.date_to as CalendarDate | undefined,
      });
  } else if (registry === 'transportations') {
    text('container_number', 'query');
    values('type', 'types');
    values('container_type', 'container_types');
    if (typeof query.hazardous === 'boolean')
      result.push({ field: 'hazardous', values: [String(query.hazardous)] });
  } else {
    text('name', 'query');
    if (query.country_code)
      result.push({ field: 'country_code', values: [String(query.country_code)] });
    if (query.role) result.push({ field: 'roles', values: [String(query.role)] });
    if (query.status === 'active' || query.status === 'archived')
      result.push({ field: 'archived', values: [String(query.status === 'archived')] });
  }
  const filters = (query.filters as ColumnFilter[] | undefined) ?? [];
  return [
    ...result.filter((old) => !filters.some((filter) => filter.field === old.field)),
    ...filters,
  ];
}
// Настройки колонок сюда не передаются: скрытие и перестановка не меняют условия и порядок.
export function withColumnFilters<T extends object>(
  registry: string,
  query: T,
  filters: ColumnFilter[],
): T & { filters: ColumnFilter[] } {
  const clear =
    registry === 'shipments'
      ? {
          number_query: undefined,
          cargo_query: undefined,
          customer_id: undefined,
          statuses: undefined,
          date_from: undefined,
          date_to: undefined,
        }
      : registry === 'transportations'
        ? { query: undefined, types: undefined, container_types: undefined, hazardous: undefined }
        : { query: '', role: undefined, country_code: undefined, status: 'all' };
  return { ...query, ...clear, filters } as T & { filters: ColumnFilter[] };
}
