import { describe, expect, it } from 'vitest';
import { initialRegistryQuery, registryRequest } from './transportationRegistry';
describe('запрос реестра перевозок', () => {
  it('передаёт курсор и все условия через filters', () => {
    const cursor = { id: 'id', value: 'mscu', scope: 'scope' };
    const filters = [
      { field: 'container_number', query: 'mscu' },
      { field: 'hazardous', values: ['false'] },
      { field: 'type', values: ['air'] },
    ];
    expect(registryRequest({ ...initialRegistryQuery, filters }, cursor)).toEqual({
      ...initialRegistryQuery,
      filters,
      cursor,
      limit: 50,
    });
  });
  it('новый запрос начинает с первой страницы', () => {
    expect(registryRequest(initialRegistryQuery)).not.toHaveProperty('cursor');
  });
});

it('передаёт группы участков без UI-подписей', () => {
  const route_filters = [
    { filters: [{ field: 'destination_station', values: ['uuid'], labels: { uuid: 'Москва' } }] },
  ];
  expect(registryRequest({ ...initialRegistryQuery, route_filters }).route_filters).toEqual([
    { filters: [{ field: 'destination_station', values: ['uuid'] }] },
  ]);
});
