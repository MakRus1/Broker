import { expect, it } from 'vitest';
import { rowAt, rowOffsets, visibleRowIndices } from './virtualRows';
it('строит смещения с учётом переменной высоты и ищет границы', () => {
  const offsets = rowOffsets(
    ['a', 'b', 'c'],
    new Map([
      ['a', 40],
      ['b', 200],
      ['c', 70],
    ]),
  );
  expect(offsets).toEqual([0, 40, 240, 310]);
  expect(rowAt(offsets, 39)).toBe(0);
  expect(rowAt(offsets, 40)).toBe(1);
  expect(rowAt(offsets, 239)).toBe(1);
  expect(rowAt(offsets, 999)).toBe(2);
});
it('число отрисованных строк не растёт с размером реестра', () => {
  for (const count of [1000, 100_000]) {
    const offsets = rowOffsets(
      Array.from({ length: count }, (_, index) => String(index)),
      new Map(),
    );
    const range = visibleRowIndices(offsets, 40_000, 800);
    expect(range.length).toBeLessThan(30);
    expect(offsets[range[0]]).toBeLessThanOrEqual(40_000);
    expect(offsets[range[range.length - 1] + 1]).toBeGreaterThanOrEqual(40_800);
  }
});
it('сохраняет строку с фокусом отдельно, не отрисовывая весь промежуток', () => {
  const offsets = rowOffsets(
    Array.from({ length: 1000 }, (_, index) => String(index)),
    new Map(),
  );
  const range = visibleRowIndices(offsets, 40_000, 800, 2);
  expect(range).toContain(2);
  expect(range.length).toBeLessThan(31);
  expect(new Set(range).size).toBe(range.length);
});
it('пустой список и конец реестра не выходят за границы', () => {
  expect(visibleRowIndices([0], 0, 800)).toEqual([]);
  const offsets = rowOffsets(['a', 'b'], new Map());
  expect(visibleRowIndices(offsets, 1_000_000, 800)).toEqual([1]);
});
