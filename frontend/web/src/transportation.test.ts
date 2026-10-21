import { describe, expect, it } from 'vitest';
import { localDateTime, routeRequest, utcDateTime } from './transportation';

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
      name: 'пустой пункт',
      route_segments: [{ ...route_segment, destination: { name: '', country_code: 'RU' } }],
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
