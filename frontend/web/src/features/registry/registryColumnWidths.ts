/** Fit the standard layout before requiring horizontal scrolling. Manual widths stay exact. */
export function fitRegistryWidths(widths: readonly number[], available: number, minimum = 120) {
  const total = widths.reduce((sum, width) => sum + width, 0);
  if (available <= 0 || total <= available) return [...widths];
  const floor = widths.length * minimum;
  if (available <= floor) return widths.map(() => minimum);
  const ratio = (available - floor) / (total - floor);
  return widths.map((width) => minimum + (width - minimum) * ratio);
}
