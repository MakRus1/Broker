import { expect, it } from 'vitest';
import { CalendarDate } from './CalendarDate';

it('сохраняет календарный день без времени и преобразования часового пояса', () => {
  const day = CalendarDate.fromISO('2000-02-29');
  expect([day.year, day.month, day.day]).toEqual([2000, 2, 29]);
  expect(day.toString()).toBe('2000-02-29');
  expect(JSON.stringify({ day })).toBe('{"day":"2000-02-29"}');
});
it.each(['2026-02-29', '2026-04-31', '0000-01-01', '25.10.2026', '2026-10-25T00:00:00Z', ''])(
  'отклоняет невалидную ISO-дату %s',
  (value) => {
    expect(() => CalendarDate.fromISO(value)).toThrow();
  },
);

it.each([
  ['2026-12-31', 1, '2027-01-01'],
  ['2024-02-28', 2, '2024-03-01'],
  ['2026-10-31', 0, '2026-10-31'],
  ['2026-03-01', -1, '2026-02-28'],
])('добавляет календарные дни %s + %s', (date, days, expected) => {
  expect(CalendarDate.fromISO(date).addDays(days).toString()).toBe(expected);
});
