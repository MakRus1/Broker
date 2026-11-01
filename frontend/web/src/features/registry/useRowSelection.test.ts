import { expect, it } from 'vitest';
import { selectRange } from './useRowSelection';
const ids = ['a', 'b', 'c', 'd'];
it('выделяет диапазон в обоих направлениях, сохраняя другие строки', () => {
  expect([...selectRange(ids, new Set(['a']), 'd', 'c', true)]).toEqual(['a', 'c', 'd']);
});
it('снимает диапазон и не меняет исходное выделение', () => {
  const initial = new Set(ids);
  expect([...selectRange(ids, initial, 'b', 'c', false)]).toEqual(['a', 'd']);
  expect(initial.size).toBe(4);
});
it('сокращение протянутого диапазона восстанавливает исходный набор', () => {
  const initial = new Set(['d']);
  expect([...selectRange(ids, initial, 'a', 'c', true)]).toEqual(['d', 'a', 'b', 'c']);
  expect([...selectRange(ids, initial, 'a', 'b', true)]).toEqual(['d', 'a', 'b']);
});
