import { expect, it } from 'vitest';
import { restoreSavedViews, sameViewState, snapshotView } from './savedViews';
import { restoreColumns } from './registryPreferences';
const columns = [{ id: 'number', visible: true, width: 200 }];
it('snapshots filters, route groups, sorting and columns without sharing mutable references', () => {
  const query = {
    filters: [{ field: 'number', values: ['S-1', 'S-2'] }],
    route_filters: [{ filters: [{ field: 'vessel', query: 'test' }] }],
    sort: 'number',
    descending: false,
  };
  const view = snapshotView('id', ' Work ', query, columns);
  query.filters[0].values.push('S-3');
  expect(view.name).toBe('Work');
  expect(view.query.filters).toEqual([{ field: 'number', values: ['S-1', 'S-2'] }]);
  expect(view.columns).not.toBe(columns);
  expect(restoreSavedViews([view])).toEqual([view]);
});
it('rejects corrupt, oversized and duplicate entries and caps the collection', () => {
  const view = snapshotView('id', 'Name', {}, columns);
  expect(
    restoreSavedViews([null, { ...view, query: [] }, { ...view, name: ' ' }, view, view]),
  ).toEqual([view]);
  expect(restoreSavedViews([{ ...view, query: { text: 'x'.repeat(24000) } }])).toEqual([]);
  expect(
    restoreSavedViews(Array.from({ length: 30 }, (_, i) => ({ ...view, id: String(i) }))),
  ).toHaveLength(20);
});
it('compares state independently of object key order and omitted optional values', () => {
  expect(sameViewState({ sort: 'id', filters: undefined }, { sort: 'id' })).toBe(true);
  expect(sameViewState({ sort: 'id', descending: false }, { descending: false, sort: 'id' })).toBe(
    true,
  );
  expect(sameViewState(columns, [{ ...columns[0], width: 220 }])).toBe(false);
});
it('restores saved columns against the current schema, retaining required and new columns', () => {
  const definitions = [
    { id: 'number', width: 200, labelKey: 'registryField.number' as const, required: true },
    { id: 'new', width: 180, labelKey: 'registryField.name' as const },
  ];
  const restored = restoreColumns(
    [
      { id: 'removed', width: 200, visible: true },
      { id: 'number', width: 1, visible: false },
    ],
    definitions,
  );
  expect(restored).toEqual([
    { id: 'number', width: 120, visible: true },
    { id: 'new', width: 180, visible: true },
  ]);
});
