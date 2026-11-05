import type { ColumnPreference } from './registryPreferences';

export interface SavedView {
  id: string;
  name: string;
  query: Record<string, unknown>;
  columns: ColumnPreference[];
}
export const maxSavedViews = 20;
export function restoreSavedViews(raw: unknown): SavedView[] {
  if (!Array.isArray(raw)) return [];
  const result: SavedView[] = [];
  for (const entry of raw.slice(0, maxSavedViews)) {
    if (
      !entry ||
      typeof entry !== 'object' ||
      typeof entry.id !== 'string' ||
      entry.id.length > 64 ||
      !entry.id ||
      typeof entry.name !== 'string' ||
      !entry.name.trim() ||
      entry.name.length > 64 ||
      !entry.query ||
      typeof entry.query !== 'object' ||
      Array.isArray(entry.query) ||
      !Array.isArray(entry.columns) ||
      JSON.stringify(entry).length > 24000 ||
      result.some((view) => view.id === entry.id)
    )
      continue;
    result.push({
      id: entry.id,
      name: entry.name.trim(),
      query: entry.query,
      columns: entry.columns.filter(
        (column: ColumnPreference) =>
          column &&
          typeof column.id === 'string' &&
          typeof column.visible === 'boolean' &&
          typeof column.width === 'number' &&
          Number.isFinite(column.width),
      ),
    });
  }
  return result;
}
/** A snapshot cannot retain mutable references to the current query or columns. */
export function snapshotView(
  id: string,
  name: string,
  query: unknown,
  columns: ColumnPreference[],
): SavedView {
  return JSON.parse(JSON.stringify({ id, name: name.trim(), query, columns }));
}
export function sameViewState(a: unknown, b: unknown): boolean {
  function canonical(value: unknown): unknown {
    if (Array.isArray(value)) return value.map(canonical);
    if (value && typeof value === 'object')
      return Object.fromEntries(
        Object.entries(value)
          .filter(([, value]) => value !== undefined)
          .sort(([a], [b]) => a.localeCompare(b))
          .map(([key, value]) => [key, canonical(value)]),
      );
    return value;
  }
  return JSON.stringify(canonical(a)) === JSON.stringify(canonical(b));
}
