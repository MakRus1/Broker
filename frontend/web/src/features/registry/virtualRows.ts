export const VIRTUAL_ROW_ESTIMATE = 88;
export const VIRTUAL_ROW_THRESHOLD = 150;
export function rowOffsets(ids: readonly string[], heights: ReadonlyMap<string, number>) {
  const offsets = [0];
  for (const id of ids)
    offsets.push(offsets[offsets.length - 1] + (heights.get(id) ?? VIRTUAL_ROW_ESTIMATE));
  return offsets;
}
export function rowAt(offsets: readonly number[], position: number) {
  let low = 0,
    high = offsets.length - 1;
  while (low < high) {
    const middle = Math.floor((low + high + 1) / 2);
    if (offsets[middle] <= position) low = middle;
    else high = middle - 1;
  }
  return Math.min(low, Math.max(0, offsets.length - 2));
}
export function visibleRowIndices(
  offsets: readonly number[],
  top: number,
  height: number,
  focused?: number,
) {
  const count = offsets.length - 1;
  if (!count) return [];
  const start = rowAt(offsets, Math.max(0, top - 600));
  const end = Math.min(count, rowAt(offsets, Math.max(0, top + height + 600)) + 1);
  const indices = Array.from({ length: end - start }, (_, index) => start + index);
  if (
    focused !== undefined &&
    focused >= 0 &&
    focused < count &&
    (focused < start || focused >= end)
  )
    indices.push(focused);
  return indices.sort((a, b) => a - b);
}
