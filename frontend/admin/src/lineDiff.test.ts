import { describe, expect, it } from 'vitest';
import { lineDiff, valueLines } from './lineDiff';

function assertReconstructs(before: unknown, after: unknown) {
  const rows = lineDiff(before, after);
  expect(rows.flatMap((row) => (row.before ? [row.before.text] : []))).toEqual(valueLines(before));
  expect(rows.flatMap((row) => (row.after ? [row.after.text] : []))).toEqual(valueLines(after));
  return rows;
}

describe('двусторонний diff JSON', () => {
  it('показывает полные значения и выделяет измененную строку с обеих сторон', () => {
    const rows = assertReconstructs(
      { default: { days: 30, enabled: false } },
      { default: { days: 60, enabled: false } },
    );
    const changed = rows.filter((row) => row.before?.kind === 'removed');
    expect(changed).toHaveLength(1);
    expect(changed[0].before?.text).toContain('30');
    expect(changed[0].after?.text).toContain('60');
    expect(changed[0].after?.kind).toBe('added');
  });
  it.each([
    [{ a: 1 }, { a: 1, b: 2 }],
    [{ a: { nested: [1, 2] } }, { a: null }],
    [{ a: [1, 2, 3] }, { a: [2, 4] }],
    [{}, { a: 'строка\nс переносом' }],
  ])('сохраняет строки при добавлении и удалении', (before, after) => {
    assertReconstructs(before, after);
  });
  it('игнорирует порядок ключей, сохраняя номера строк', () => {
    const rows = assertReconstructs({ a: 1, b: 2 }, { b: 2, a: 1 });
    expect(rows.every((row) => row.before?.kind === 'same' && row.after?.kind === 'same')).toBe(
      true,
    );
    expect(rows.map((row) => row.before?.line)).toEqual([1, 2, 3, 4]);
  });
  it('ограничивает память для больших документов, сохраняя обе стороны', () => {
    const before = Array.from({ length: 1200 }, (_, index) => index);
    const after = [...before];
    after[600] = 10000;
    const rows = assertReconstructs(before, after);
    expect(rows.filter((row) => row.before?.kind === 'removed')).toHaveLength(1);
  });
});
