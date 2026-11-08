import { describe, expect, it } from 'vitest';
import { hasWorkFilter, salesWorkFilters, toggleWorkFilter } from './salesWorkFilters';
import {
  filterRequest,
  restoreColumnFilters,
  type ColumnFilter,
} from '../registry/registryFilters';

describe('sales work filters', () => {
  const options = salesWorkFilters('manager', 'Manager', new Date(2026, 0, 1, 0, 1));
  it('uses the local calendar date and excludes today from overdue', () => {
    expect(options[1].filter.date_to.toString()).toBe('2025-12-31');
    expect(options[2].filter.date_from.toString()).toBe('2026-01-01');
    expect(options[2].filter.date_to.toString()).toBe('2026-01-01');
  });
  it('combines independent filters, replaces date ranges and toggles off', () => {
    let filters: ColumnFilter[] = [{ field: 'stage_id', values: ['stage'] }];
    filters = toggleWorkFilter(filters, options[0].filter);
    filters = toggleWorkFilter(filters, options[1].filter);
    filters = toggleWorkFilter(filters, options[4].filter);
    expect(filters).toHaveLength(4);
    filters = toggleWorkFilter(filters, options[2].filter);
    expect(hasWorkFilter(filters, options[1].filter)).toBe(false);
    expect(hasWorkFilter(filters, options[2].filter)).toBe(true);
    expect(hasWorkFilter(filters, options[0].filter)).toBe(true);
    filters = toggleWorkFilter(filters, options[2].filter);
    expect(filters.map((filter) => filter.field)).toEqual([
      'stage_id',
      'manager_subject',
      'has_contacts',
    ]);
  });
  it('restores date objects while preferences are awaiting server persistence', () => {
    const restored = restoreColumnFilters([options[0].filter, options[2].filter]);
    expect(hasWorkFilter(restored, options[0].filter)).toBe(true);
    expect(hasWorkFilter(restored, options[2].filter)).toBe(true);
  });
  it('round-trips through saved query settings without a second filter mechanism', () => {
    const filters = options
      .filter((option) => option.id !== 'overdue')
      .map((option) => option.filter);
    const restored = restoreColumnFilters(JSON.parse(JSON.stringify(filters)));
    for (const option of options.filter((option) => option.id !== 'overdue'))
      expect(hasWorkFilter(restored, option.filter)).toBe(true);
    expect(
      filterRequest(restored)?.find((filter) => filter.field === 'manager_subject'),
    ).not.toHaveProperty('labels');
  });
});
