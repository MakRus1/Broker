export const VIRTUAL_COLUMN_THRESHOLD = 16;

export interface ColumnSlot {
  index?: number;
  width: number;
}

// Разрывы также становятся колонками colgroup: браузер сохраняет полную ширину таблицы.
export function columnSlots(
  widths: readonly number[],
  left: number,
  viewport: number,
  pinned: readonly number[] = [],
): ColumnSlot[] {
  left = Math.min(
    Math.max(0, left),
    Math.max(0, widths.reduce((sum, width) => sum + width, 0) - viewport),
  );
  const selected = new Set(pinned);
  let offset = 0;
  const slots: ColumnSlot[] = [];
  widths.forEach((width, index) => {
    const end = offset + width;
    const show = selected.has(index) || (end > left - 320 && offset < left + viewport + 320);
    if (show) slots.push({ index, width });
    else if (slots.length && slots[slots.length - 1].index === undefined)
      slots[slots.length - 1].width += width;
    else slots.push({ width });
    offset = end;
  });
  return slots;
}
