import { describe, expect, it } from 'vitest';
import { renderToStaticMarkup } from 'react-dom/server';
import { CalendarDate } from './CalendarDate';
import {
  filterRequest,
  restoreColumnFilters,
  splitFilterValues,
  withColumnFilters,
  replaceFilters,
} from './registryFilters';
import { restoreColumns } from './registryPreferences';
import { RegistryTable } from './RegistryTable';
import type { ColumnFilterController } from './RegistryColumnFilters';
describe('фильтры заголовков реестра', () => {
  it('разбирает вставку из Excel и сохраняет значения со пробелами', () => {
    expect(splitFilterValues('S-1\tS-2\nS-1; ABC 25, S-3')).toEqual([
      'S-1',
      'S-2',
      'ABC 25',
      'S-3',
    ]);
  });
  it('сохраняет типы дат и не отправляет подписи значений в сервис', () => {
    const decoded = restoreColumnFilters([
      { field: 'shipment_date', date_from: '2026-01-01' },
      { field: 'number', values: ['s-2'], labels: { 's-2': 'S-2' } },
    ])!;
    expect(decoded[0].date_from).toBeInstanceOf(CalendarDate);
    expect(filterRequest(decoded)?.[1]).not.toHaveProperty('labels');
    expect(
      restoreColumnFilters([{ field: 'shipment_date', date_from: '2026-02-30' }]),
    ).toBeUndefined();
  });
  it('применяет условие одного поля, сохраняя условия других полей и сортировку', () => {
    const query = {
      sort: 'number',
      descending: true,
      number_query: 'old',
      filters: [
        { field: 'number', values: ['s-1'] },
        { field: 'status', values: ['Новая'] },
      ],
    };
    const updated = withColumnFilters(
      'shipments',
      query,
      replaceFilters(query.filters, ['number'], [{ field: 'number', values: ['s-2', 's-3'] }]),
    );
    expect(updated.sort).toBe('number');
    expect(updated.descending).toBe(true);
    expect(updated.number_query).toBeUndefined();
    expect(updated.filters).toEqual([
      { field: 'status', values: ['Новая'] },
      { field: 'number', values: ['s-2', 's-3'] },
    ]);
  });
  it('скрытие столбца не сбрасывает фильтр или сортировку и оставляет видимый индикатор', () => {
    const definitions = [
      { id: 'number', label: 'Поставка', width: 180, render: () => '' },
      { id: 'status', label: 'Статус', width: 180, render: () => '' },
    ];
    const query = {
      sort: 'number',
      descending: true,
      filters: [{ field: 'number', values: ['s-2'], labels: { 's-2': 'S-2' } }],
    };
    const original = JSON.stringify(query);
    const columns = restoreColumns([{ id: 'number', visible: false, width: 180 }], definitions);
    const controller: ColumnFilterController = {
      fields: [{ id: 'number', label: 'Поставка', kind: 'values', sortable: true }],
      ...query,
      failure: '',
      change: () => {},
      order: () => {},
      values: async () => ({ values: [] }),
    };
    const html = renderToStaticMarkup(
      <RegistryTable
        items={[]}
        definitions={definitions}
        columns={{ columns, definitions, setColumns: () => {} }}
        sort={query.sort}
        descending={query.descending}
        onSort={() => {}}
        onOpen={() => {}}
        filters={controller}
      />,
    );
    expect(JSON.stringify(query)).toBe(original);
    expect(html).toContain('скрытый столбец');
    expect(html).toContain('S-2');
    expect(html).toContain('Порядок:');
    expect(html).not.toContain('data-column="number"');
  });
});
