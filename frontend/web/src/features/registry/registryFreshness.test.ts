import { describe, expect, it } from 'vitest';
import { RegistryFreshness, REGISTRY_MAX_AGE_MS } from './registryFreshness';

describe('актуальность загруженного реестра', () => {
  it('сохраняет страницы при быстром возврате, но ограничивает возраст', () => {
    const cache = new RegistryFreshness();
    const query = { filters: [] };
    expect(cache.isFresh([query], 0, 100)).toBe(false);
    cache.complete([query], 0, 100);
    expect(cache.isFresh([query], 0, 100 + REGISTRY_MAX_AGE_MS - 1)).toBe(true);
    expect(cache.isFresh([query], 0, 100 + REGISTRY_MAX_AGE_MS)).toBe(false);
    expect(cache.isFresh([query], 0, 99)).toBe(false);
  });
  it('смена фильтра, пользователя, области или ручной ревизии требует загрузки', () => {
    const cache = new RegistryFreshness();
    const query = {},
      session = {};
    cache.complete([query, session, 'scope-a', 1], 0, 100);
    for (const identity of [
      [{}, session, 'scope-a', 1],
      [query, {}, 'scope-a', 1],
      [query, session, 'scope-b', 1],
      [query, session, 'scope-a', 2],
    ])
      expect(cache.isFresh(identity, 0, 101)).toBe(false);
  });
  it('запись во время запроса не делает его ответ актуальным', () => {
    const cache = new RegistryFreshness();
    cache.complete(['query'], 3, 100);
    expect(cache.isFresh(['query'], 4, 101)).toBe(false);
  });
  it('ошибка или отмена повторной загрузки не оставляет старую отметку успеха', () => {
    const cache = new RegistryFreshness();
    cache.complete(['query'], 0, 100);
    cache.clear();
    expect(cache.isFresh(['query'], 0, 101)).toBe(false);
  });
});
