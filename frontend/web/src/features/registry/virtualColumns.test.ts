import { expect, it } from 'vitest';
import { columnSlots } from './virtualColumns';

it('сохраняет полную ширину и ограничивает число ячеек в середине и конце', () => {
  const widths = Array.from({ length: 100 }, (_, i) => 120 + i);
  for (const left of [0, 8000, 16000]) {
    const slots = columnSlots(widths, left, 900);
    expect(slots.reduce((sum, slot) => sum + slot.width, 0)).toBe(
      widths.reduce((a, b) => a + b, 0),
    );
    expect(slots.filter((slot) => slot.index !== undefined).length).toBeLessThan(16);
    for (const slot of slots)
      if (slot.index !== undefined) expect(slot.width).toBe(widths[slot.index]);
  }
});
it('сохраняет отдельную колонку с фокусом без всех промежуточных колонок', () => {
  const slots = columnSlots(Array(100).fill(160), 8000, 900, [0, 99]);
  expect(slots.some((slot) => slot.index === 0)).toBe(true);
  expect(slots.some((slot) => slot.index === 99)).toBe(true);
  expect(slots.length).toBeLessThan(18);
  expect(slots.reduce((sum, slot) => sum + slot.width, 0)).toBe(16000);
});
it('после изменения ширины не теряет и не удваивает место соседних колонок', () => {
  const slots = columnSlots([120, 900, 180, 160], 1100, 200);
  expect(slots.reduce((sum, slot) => sum + slot.width, 0)).toBe(1360);
  expect(slots.filter((slot) => slot.index !== undefined).map((slot) => slot.index)).toContain(2);
  expect(columnSlots([], 0, 100)).toEqual([]);
});

it('при скрытии колонок за текущей позицией не оставляет пустое окно', () => {
  const slots = columnSlots([160, 160, 160], 10000, 320);
  expect(slots.some((slot) => slot.index === 2)).toBe(true);
  expect(slots.reduce((sum, slot) => sum + slot.width, 0)).toBe(480);
});
