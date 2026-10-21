import { describe, expect, it } from 'vitest';
import { localDateTime, routeRequest, utcDateTime } from './transportation';

const leg = {
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
      { ...leg, actual_departure_at: '2026-10-21T10:30:00Z' } as typeof leg,
    ]);
    expect(result.expected_revision).toBe(7);
    expect(result.legs[0].id).toBe(leg.id);
    expect(result.legs[0].origin).toEqual({ name: 'Шанхай', country_code: 'CN' });
    expect(result.legs[0]).not.toHaveProperty('actual_departure_at');
    expect(leg.origin.name).toBe(' Шанхай ');
  });
  it.each([
    { name: 'пустой маршрут', legs: [] },
    { name: 'пустой пункт', legs: [{ ...leg, destination: { name: '', country_code: 'RU' } }] },
    { name: 'повтор ID', legs: [leg, leg] },
    { name: 'обратный интервал', legs: [{ ...leg, planned_arrival_at: '2026-10-20T10:00:00Z' }] },
  ])('отклоняет $name', ({ legs }) => {
    expect(() => routeRequest('transportation', 1, legs)).toThrow();
  });
  it('переводит время из локального поля в UTC и обратно без сдвига', () => {
    const input = '2026-01-15T14:30';
    expect(localDateTime(utcDateTime(input))).toBe(input);
    expect(() => utcDateTime('')).toThrow();
  });
});
