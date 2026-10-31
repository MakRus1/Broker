import { CalendarDate } from '../../shared/dates/CalendarDate';
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

/** Ограничение вложенного реестра: его поля нельзя снять локальными фильтрами. */
export type RegistryScope = { key: string; filters: ColumnFilter[] };
export function scopedFilters(filters: ColumnFilter[] | undefined, scope?: RegistryScope) {
  const fields = new Set(scope?.filters.map((filter) => filter.field));
  return [
    ...(filters ?? []).filter((filter) => !fields.has(filter.field)),
    ...(scope?.filters ?? []),
  ];
}
