import { CalendarDate } from '../../shared/dates/CalendarDate';
import type { ColumnFilter } from '../registry/registryFilters';

export function salesWorkFilters(subject: string, displayName: string, now = new Date()) {
  const today = CalendarDate.fromISO(
    `${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, '0')}-${String(now.getDate()).padStart(2, '0')}`,
  );
  return [
    {
      id: 'mine',
      filter: { field: 'manager_subject', values: [subject], labels: { [subject]: displayName } },
    },
    { id: 'overdue', filter: { field: 'next_action_date', date_to: today.addDays(-1) } },
    { id: 'today', filter: { field: 'next_action_date', date_from: today, date_to: today } },
    { id: 'noAction', filter: { field: 'next_action', empty: true } },
    { id: 'noContacts', filter: { field: 'has_contacts', values: ['false'] } },
  ] as const satisfies readonly { id: string; filter: ColumnFilter }[];
}
export function hasWorkFilter(filters: ColumnFilter[] | undefined, expected: ColumnFilter) {
  return (filters ?? []).some(
    (filter) =>
      filter.field === expected.field &&
      JSON.stringify(filter.values) === JSON.stringify(expected.values) &&
      filter.empty === expected.empty &&
      filter.query === expected.query &&
      filter.date_from?.toString() === expected.date_from?.toString() &&
      filter.date_to?.toString() === expected.date_to?.toString(),
  );
}
export function toggleWorkFilter(
  filters: ColumnFilter[] | undefined,
  expected: ColumnFilter,
): ColumnFilter[] {
  const others = (filters ?? []).filter((filter) => filter.field !== expected.field);
  return hasWorkFilter(filters, expected) ? others : [...others, expected];
}
