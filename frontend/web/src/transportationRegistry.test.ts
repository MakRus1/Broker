import { describe, expect, it } from 'vitest';
import { initialRegistryQuery, registryRequest } from './transportationRegistry';

describe('запрос реестра перевозок', () => {
  it('передаёт курсор и фильтры серверу, не вычисляет страницы во фронтенде', () => {
    const cursor = { id: 'id', value: 'mscu', scope: 'scope' };
    expect(
      registryRequest(
        { ...initialRegistryQuery, query: ' mscu ', hazardous: false, types: ['air'] },
        cursor,
      ),
    ).toEqual({
      ...initialRegistryQuery,
      query: 'mscu',
      hazardous: false,
      types: ['air'],
      cursor,
      limit: 50,
    });
  });
  it('новый запрос начинает с первой страницы', () => {
    expect(registryRequest({ ...initialRegistryQuery, query: '  ' })).not.toHaveProperty('cursor');
    expect(registryRequest({ ...initialRegistryQuery, query: '  ' }).query).toBeUndefined();
  });
});
