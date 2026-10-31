import { describe, expect, it } from 'vitest';
import { diffSections } from './diffSections';
import type { DiffRow } from './lineDiff';

function rows(count: number, changed: number[] = []): DiffRow[] {
  return Array.from({ length: count }, (_, index) => ({
    before: {
      text: `${index}`,
      line: index + 1,
      kind: changed.includes(index) ? 'removed' : 'same',
    },
    after: { text: `${index}`, line: index + 1, kind: changed.includes(index) ? 'added' : 'same' },
  }));
}

describe('сворачивание неизмененных участков diff', () => {
  it('сохраняет контекст вокруг каждого изменения', () => {
    expect(diffSections(rows(20, [10]))).toEqual([
      { start: 0, end: 8, collapsed: true },
      { start: 8, end: 13, collapsed: false },
      { start: 13, end: 20, collapsed: true },
    ]);
  });
  it('объединяет пересекающийся контекст и не скрывает изменения', () => {
    const sections = diffSections(rows(20, [5, 8]));
    for (const index of [5, 8])
      expect(sections.find((part) => part.start <= index && part.end > index)?.collapsed).toBe(
        false,
      );
    expect(sections.find((part) => part.start === 3)?.end).toBe(11);
  });
  it('сворачивает полностью одинаковый документ', () => {
    expect(diffSections(rows(10))).toEqual([{ start: 0, end: 10, collapsed: true }]);
  });
  it('оставляет короткие участки видимыми и не теряет строки', () => {
    const sections = diffSections(rows(5, [2]));
    expect(sections).toEqual([{ start: 0, end: 5, collapsed: false }]);
    expect(diffSections([])).toEqual([]);
  });
});
