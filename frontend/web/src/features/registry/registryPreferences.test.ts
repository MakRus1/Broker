import { describe, expect, it } from 'vitest';
import { moveColumn, placeColumn, preferenceKey, restoreColumns } from './registryPreferences';
import {
  restoreShipmentQuery,
  restoreTransportationQuery,
  restoreCounterpartyQuery,
} from './registryQueryPreferences';
import { CalendarDate } from '../../shared/dates/CalendarDate';

const definitions = [
  { id: 'name', label: 'Наименование', width: 300, required: true },
  { id: 'country', label: 'Страна', width: 150 },
  { id: 'tax', label: 'ИНН', width: 200 },
];
describe('личная раскладка реестра', () => {
  it('сохраняет порядок и скрытие, исключает удалённые/повторные поля, добавляет новые', () => {
    expect(
      restoreColumns(
        [
          { id: 'country', visible: false, width: 240 },
          { id: 'removed', visible: true, width: 200 },
          { id: 'name', visible: false, width: 340 },
          { id: 'country', visible: true, width: 150 },
        ],
        definitions,
      ),
    ).toEqual([
      { id: 'country', visible: false, width: 240 },
      { id: 'name', visible: true, width: 340 },
      { id: 'tax', visible: true, width: 200 },
    ]);
  });
  it('восстанавливает безопасные ширины и значения из повреждённых настроек', () => {
    const restored = restoreColumns(
      [
        null,
        1,
        { id: 'name', width: 1 },
        { id: 'country', width: Infinity },
        { id: 'tax', width: 5000 },
      ],
      definitions,
    );
    expect(restored.map((column) => column.width)).toEqual([120, 150, 900]);
    expect(restoreColumns({}, definitions).map((column) => column.id)).toEqual([
      'name',
      'country',
      'tax',
    ]);
  });
  it('переставляет также скрытые поля, не теряя ширину и не выходя за границу', () => {
    const columns = restoreColumns(null, definitions);
    expect(moveColumn(columns, 'tax', -1).map((column) => column.id)).toEqual([
      'name',
      'tax',
      'country',
    ]);
    expect(moveColumn(columns, 'name', -1)).toBe(columns);
    expect(moveColumn(columns, 'missing', 1)).toBe(columns);
  });
  it('изолирует пользователей, реестры и типы настроек', () => {
    expect(
      new Set([
        preferenceKey('a', 'shipments', 'columns'),
        preferenceKey('b', 'shipments', 'columns'),
        preferenceKey('a', 'transportations', 'columns'),
        preferenceKey('a', 'shipments', 'query'),
      ]).size,
    ).toBe(4);
    expect(preferenceKey('', 'shipments', 'columns')).toBe('');
  });
});
describe('восстановление применённых фильтров', () => {
  it('восстанавливает календарные даты как CalendarDate после JSON', () => {
    const restored = restoreShipmentQuery(
      JSON.parse(
        JSON.stringify({
          filters: [
            { field: 'number', query: 'LCB' },
            { field: 'shipment_date', date_from: CalendarDate.fromISO('2026-10-01') },
          ],
          sort: 'shipment_date',
          descending: true,
        }),
      ),
    );
    expect(restored.filters?.[1].date_from).toBeInstanceOf(CalendarDate);
    expect(restored.filters?.[1].date_from?.toDisplayString()).toBe('01.10.2026');
    expect(restored.filters?.[0].query).toBe('LCB');
  });
  it('отбрасывает неверные фильтры и старый формат; не восстанавливает курсор', () => {
    const query = restoreShipmentQuery({
      filters: [{ field: 'shipment_date', date_from: '2026-02-30' }],
      number_query: 'old',
      customer_id: 'old',
      sort: 'sql',
      cursor: { id: 'old' },
    });
    expect(query.filters).toBeUndefined();
    expect(query.sort).toBe('sql');
    expect(query).not.toHaveProperty('number_query');
    expect(query).not.toHaveProperty('customer_id');
    expect(query).not.toHaveProperty('cursor');
  });
  it('сохраняет несколько значений и явное состояние контрагента', () => {
    const filters = [
      { field: 'type', values: ['consolidated', 'air'] },
      { field: 'hazardous', values: ['false'] },
    ];
    expect(restoreTransportationQuery({ filters, sort: 'type', descending: true })).toEqual({
      filters,
      sort: 'type',
      descending: true,
    });
    const partyFilters = [
      { field: 'archived', values: ['true'] },
      { field: 'roles', values: ['carrier'] },
      { field: 'country_code', values: ['RU'] },
    ];
    expect(restoreCounterpartyQuery({ filters: partyFilters, sort: 'tax_id' })).toMatchObject({
      filters: partyFilters,
      sort: 'tax_id',
    });
  });
});

it('вставляет столбцы до/после цели в обоих направлениях, включая конец', () => {
  const columns = restoreColumns(null, definitions);
  expect(placeColumn(columns, 'name', 'tax', 'after').map((column) => column.id)).toEqual([
    'country',
    'tax',
    'name',
  ]);
  expect(placeColumn(columns, 'tax', 'name', 'before').map((column) => column.id)).toEqual([
    'tax',
    'name',
    'country',
  ]);
  expect(placeColumn(columns, 'name', 'country', 'before')).toEqual(columns);
  expect(placeColumn(columns, 'name', 'name', 'after')).toBe(columns);
  expect(placeColumn(columns, 'missing', 'name', 'before')).toBe(columns);
  expect(placeColumn(columns, 'name', 'missing', 'before')).toBe(columns);
  expect(placeColumn(columns, 'name', 'tax', 'after')[2]).toBe(columns[0]);
});

it('сохраняет группы участков независимо от видимости столбцов и восстанавливает даты', () => {
  const route_filters = [
    {
      filters: [
        { field: 'mode', values: ['rail'] },
        { field: 'eta', date_from: '2026-10-01' },
      ],
    },
  ];
  const restored = restoreTransportationQuery({ route_filters, sort: 'container_number' });
  expect(restored.route_filters?.[0].filters[1].date_from).toBeInstanceOf(CalendarDate);
  expect(restored.route_filters?.[0].filters[0].values).toEqual(['rail']);
  expect(
    restoreTransportationQuery({ route_filters: [{ filters: [] }] }).route_filters,
  ).toBeUndefined();
});
