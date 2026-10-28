import { describe, expect, it } from 'vitest';
import { moveColumn, placeColumn, preferenceKey, restoreColumns } from './registryPreferences';
import {
  restoreShipmentQuery,
  restoreTransportationQuery,
  restoreCounterpartyQuery,
} from './registryQueryPreferences';
import { CalendarDate } from './CalendarDate';

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
    const query = {
      number_query: 'LCB',
      date_from: CalendarDate.fromISO('2026-10-01'),
      sort: 'shipment_date',
      descending: true,
    };
    const restored = restoreShipmentQuery(JSON.parse(JSON.stringify(query)));
    expect(restored.date_from).toBeInstanceOf(CalendarDate);
    expect(restored.date_from?.toDisplayString()).toBe('01.10.2026');
    expect(restored.number_query).toBe('LCB');
  });
  it('отбрасывает неверные даты, сортировки и фильтры; не восстанавливает курсор', () => {
    const query = restoreShipmentQuery({
      date_from: '2026-02-30',
      sort: 'sql',
      customer_id: 'wrong',
      cursor: { id: 'old' },
    });
    expect(query.date_from).toBeUndefined();
    expect(query.sort).toBe('sql'); // Поддержку поля проверяет серверный каталог, а не второй список во фронте.
    expect(query.customer_id).toBeUndefined();
    expect(query).not.toHaveProperty('cursor');
  });
  it('сохраняет false и поддерживаемые типы перевозки', () => {
    expect(
      restoreTransportationQuery({
        hazardous: false,
        types: ['consolidated', 'wrong'],
        sort: 'type',
        descending: true,
      }),
    ).toMatchObject({ hazardous: false, types: ['consolidated'], sort: 'type', descending: true });
    expect(
      restoreCounterpartyQuery({
        status: 'archived',
        role: 'carrier',
        country_code: 'RU',
        sort: 'tax_id',
      }),
    ).toMatchObject({ status: 'archived', role: 'carrier', country_code: 'RU', sort: 'tax_id' });
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
