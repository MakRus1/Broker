import { describe, expect, it } from 'vitest';
import { changes, editorState, hasUnsavedChanges } from './changes';

describe('сравнение значений', () => {
  it('не считает порядок ключей и пробелы изменением', () => {
    expect(changes({ a: 1, b: [true, null] }, { b: [true, null], a: 1 })).toEqual([]);
    expect(editorState('{ "b": 2, "a": 1 }', { a: 1, b: 2 }, { a: 1, b: 2 }).changed).toBe(false);
  });
  it('показывает добавление, удаление и замену по отдельным путям', () => {
    expect(
      changes({ a: null, b: 1, nested: { x: false } }, { a: false, c: 2, nested: { x: true } }),
    ).toEqual([
      { path: '$["a"]', before: null, after: false, kind: 'changed' },
      { path: '$["b"]', before: 1, kind: 'removed' },
      { path: '$["c"]', after: 2, kind: 'added' },
      { path: '$["nested"]["x"]', before: false, after: true, kind: 'changed' },
    ]);
  });
  it('сравнивает массивы по индексам и различает массив и объект', () => {
    expect(changes([1], [2, 3])).toHaveLength(2);
    expect(changes([null], [])).toEqual([{ path: '$[0]', before: null, kind: 'removed' }]);
    expect(changes([], {})).toHaveLength(1);
  });
  it('не позволяет свести драфт к опубликованному значению', () => {
    expect(editorState('{"x": 1}', { x: 2 }, { x: 1 }).changed).toBe(false);
    expect(editorState('{"x": 3}', { x: 2 }, { x: 1 }).changed).toBe(true);
  });
  it.each(['{', '[]', 'null', '3'])('объясняет невалидное значение %s', (text) => {
    expect(editorState(text, {}, {}).error).not.toBe('');
  });
});

describe('защита несохранённых изменений', () => {
  it('не предупреждает из-за форматирования или порядка ключей', () => {
    expect(hasUnsavedChanges('{ "b": 2, "a": 1 }', { a: 1, b: 2 }, '', '')).toBe(false);
  });
  it.each(['{"a": 2}', '{', 'null'])('защищает изменённое или невалидное значение %s', (text) => {
    expect(hasUnsavedChanges(text, { a: 1 }, '', '')).toBe(true);
  });
  it('защищает отдельно изменённую причину', () => {
    expect(hasUnsavedChanges('{"a": 1}', { a: 1 }, 'Новая причина', 'Старая причина')).toBe(true);
    expect(hasUnsavedChanges('{"a": 1}', { a: 1 }, 'Старая причина', 'Старая причина')).toBe(false);
  });
  it('защищает возврат редактируемого драфта к опубликованному значению', () => {
    expect(hasUnsavedChanges('{"a": 1}', { a: 2 }, '', '')).toBe(true);
  });
});
