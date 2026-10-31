import { describe, expect, it } from 'vitest';
import {
  containerNumberWarning,
  transportationTitle,
  localDateTime,
  routeRequest,
  routePoint,
  utcDateTime,
} from './transportation';

const route_segment = {
  id: '11111111-1111-4111-8111-111111111111',
  mode: 'sea' as const,
  origin: { name: ' Шанхай ', country_code: 'cn' },
  destination: { name: 'Владивосток', country_code: 'ru' },
  planned_departure_at: '2026-10-21T10:00:00Z',
  planned_arrival_at: '2026-10-23T10:00:00Z',
};

describe('Маршрут и время', () => {
  it('отправляет стабильный ID и редакцию, нормализует пункты и исключает факты из маршрута', () => {
    const result = routeRequest('transportation', 7, [
      { ...route_segment, actual_departure_at: '2026-10-21T10:30:00Z' } as typeof route_segment,
    ]);
    expect(result.expected_revision).toBe(7);
    expect(result.route_segments[0].id).toBe(route_segment.id);
    expect(result.route_segments[0].origin).toEqual({ name: 'Шанхай', country_code: 'CN' });
    expect(result.route_segments[0]).not.toHaveProperty('actual_departure_at');
    expect(route_segment.origin.name).toBe(' Шанхай ');
  });
  it.each([
    { name: 'пустой маршрут', route_segments: [] },
    {
      name: 'некорректный код страны',
      route_segments: [{ ...route_segment, destination: { name: '', country_code: 'RUS' } }],
    },
    { name: 'повтор ID', route_segments: [route_segment, route_segment] },
    {
      name: 'обратный интервал',
      route_segments: [{ ...route_segment, planned_arrival_at: '2026-10-20T10:00:00Z' }],
    },
  ])('отклоняет $name', ({ route_segments }) => {
    expect(() => routeRequest('transportation', 1, route_segments)).toThrow();
  });
  it('переводит время из локального поля в UTC и обратно без сдвига', () => {
    const input = '2026-01-15T14:30';
    expect(localDateTime(utcDateTime(input))).toBe(input);
    expect(() => utcDateTime('')).toThrow();
  });
});

it.each(['2026-02-30T12:00', '2026-13-01T12:00', '2026-01-01T25:00', '0000-01-01T12:00'])(
  'отклоняет некорректную календарную дату %s',
  (value) => {
    expect(() => utcDateTime(value)).toThrow();
  },
);
it('не ограничивает год произвольным бизнес-диапазоном', () => {
  expect(localDateTime(utcDateTime('1111-11-11T11:11'))).toBe('1111-11-11T11:11');
  expect(localDateTime(utcDateTime('2030-01-01T12:00'))).toBe('2030-01-01T12:00');
});

it.each(['CSQU3054383', 'ABCD1234567', 'csqu3054383', '', '  '])(
  'номер %s не требует предупреждения',
  (value) => {
    expect(containerNumberWarning(value)).toBe('');
  },
);
it.each(['CSQU305438', 'CSQU30543833', 'АБВГ1234567', 'ожидается'])(
  'показывает подсказку для %s',
  (value) => {
    expect(containerNumberWarning(value)).not.toBe('');
  },
);
it('использует фактический номер контейнера и понятное название без номера', () => {
  expect(transportationTitle({ type: 'container', container_number: 'CSQU3054383' })).toBe(
    'CSQU3054383',
  );
  expect(transportationTitle({ type: 'container' })).toBe('Контейнер · Без номера');
  expect(transportationTitle({ type: 'air' })).toBe('Авиа · Без номера');
});

it('сохраняет календарные даты и выбранные станции ЖД без преобразования в UTC', () => {
  const rail = {
    origin_station: {
      id: '34996a7f-8d2d-1d6c-d1ec-75950190cc6d',
      esr_code: '000001',
      name: 'Станция',
      country_code: 'RU',
    },
    etd: '2030-01-01',
    eta: '2030-01-03',
    departure_date: '2020-01-01',
    unloading_date: '2020-01-04',
  };
  const result = routeRequest('id', 3, [{ ...route_segment, mode: 'rail', rail }]);
  expect(result.route_segments[0].rail).toEqual(rail);
  expect(result.route_segments[0]).not.toHaveProperty('origin');
  expect(result.route_segments[0]).not.toHaveProperty('destination');
  expect(routePoint(result.route_segments[0], 'origin')).toEqual(rail.origin_station);
  expect(routePoint(result.route_segments[0], 'destination').name).toBe('');
});

it('не использует старые свободные пункты в ЖД-участке', () => {
  const stage = { ...route_segment, mode: 'rail' as const };
  const result = routeRequest('id', 1, [stage]).route_segments[0];
  expect(result).not.toHaveProperty('origin');
  expect(result).not.toHaveProperty('destination');
  expect(routePoint(stage, 'origin').name).toBe('');
});

it('читает морские и авиационные пункты из поставки и не отправляет их в редактирование перевозки', () => {
  const location = { id: 'port', kind: 'sea_port' as const, name: 'Порт', country_code: 'RU' };
  const stage = { ...route_segment, sea: { pol: location, eta: '2030-01-01' } };
  expect(routePoint(stage, 'origin').name).toBe('Порт');
  expect(routeRequest('id', 1, [stage]).route_segments[0]).not.toHaveProperty('sea');
  const air = {
    ...route_segment,
    mode: 'air' as const,
    air: { origin_airport: { ...location, kind: 'airport' as const }, flight_number: 'SU123' },
  };
  expect(routePoint(air, 'origin').name).toBe('Порт');
  expect(routeRequest('id', 1, [air]).route_segments[0]).not.toHaveProperty('air');
});

it('сохраняет поля автоучастков и не подставляет город или адрес', () => {
  const pickup = { ...route_segment, mode: 'pickup' as const };
  const empty = routeRequest('transportation', 1, [pickup]).route_segments[0];
  expect(empty.pickup).toBeUndefined();
  const details = { delivery_city: 'Казань', delivery_address: 'Склад 2', driver: 'Иванов И. И.' };
  const filled = routeRequest('transportation', 1, [{ ...pickup, pickup: details }]);
  expect(filled.route_segments[0].pickup).toEqual(details);
  const road = { vehicle_number: 'А123АА', trailer_number: 'В456ВВ', cmr_number: 'CMR-1' };
  const international = routeRequest('transportation', 1, [
    { ...route_segment, mode: 'international_road', international_road: road },
  ]);
  expect(international.route_segments[0].international_road).toEqual(road);
});

it('не называет пункты автомобильных участков станциями', async () => {
  const { routeDirection } = await import('./transportation');
  expect(routeDirection({ ...route_segment, mode: 'international_road' })).toBe(
    'Место погрузки не указано',
  );
  expect(
    routeDirection({ ...route_segment, mode: 'pickup', pickup: { delivery_city: 'Находка' } }),
  ).toBe('Находка');
  expect(routeDirection({ ...route_segment, mode: 'road' })).toBe('Место доставки не указано');
  expect(routeDirection({ id: route_segment.id, mode: 'rail' })).toBe(
    'Станция не выбрана → Станция не выбрана',
  );
});
