import { describe, expect, it } from 'vitest';
import { displayDateInput, parseDateInput, maskDateInput } from './dateText';

describe('Ввод дат с клавиатуры', () => {
  it.each(['25.10.2026', '25102026', '2026-10-25', ' 25.10.2026 '])('принимает %s', (text) => {
    expect(parseDateInput(text)).toBe('2026-10-25');
  });
  it.each(['25.10.2026 09:15', '251020260915', '2026-10-25T09:15'])(
    'принимает дату и время %s',
    (text) => {
      expect(parseDateInput(text, true)).toBe('2026-10-25T09:15');
    },
  );
  it.each([
    '31.04.2026',
    '29.02.1900',
    '00.10.2026',
    '25.00.2026',
    '25.13.2026',
    '25.10.0000',
    '25.10.26',
    '25.10.',
    'случайный текст',
  ])('не принимает %s', (text) => {
    expect(parseDateInput(text)).toBeUndefined();
  });
  it('проверяет високосные годы и время без преобразования часового пояса', () => {
    expect(parseDateInput('29.02.2000')).toBe('2000-02-29');
    expect(parseDateInput('25.10.2026 24:00', true)).toBeUndefined();
    expect(parseDateInput('25.10.2026 09:60', true)).toBeUndefined();
    expect(displayDateInput('2026-10-25T09:15')).toBe('25.10.2026 09:15');
    expect(parseDateInput('')).toBe('');
  });
});

it('добавляет разделители сразу при вводе дня, месяца и времени', () => {
  expect(maskDateInput('25', 2)).toEqual({ text: '25.', cursor: 3 });
  expect(maskDateInput('25.10', 5)).toEqual({ text: '25.10.', cursor: 6 });
  expect(maskDateInput('25102026', 8)).toEqual({ text: '25.10.2026', cursor: 10 });
  expect(maskDateInput('251020260915', 12, true)).toEqual({ text: '25.10.2026 09:15', cursor: 16 });
});
it('позволяет стирать разделители и сохраняет курсор при правке в середине', () => {
  expect(maskDateInput('25', 2, false, true)).toEqual({ text: '25', cursor: 2 });
  expect(maskDateInput('25.10', 5, false, true)).toEqual({ text: '25.10', cursor: 5 });
  expect(maskDateInput('26.10.2026', 2)).toEqual({ text: '26.10.2026', cursor: 3 });
  expect(maskDateInput('', 0, false, true)).toEqual({ text: '', cursor: 0 });
  expect(maskDateInput('2026-10-25', 10)).toEqual({ text: '25.10.2026', cursor: 10 });
});

it('удаление и замена цифр месяца не затрагивают год', () => {
  const removed = maskDateInput('11.1.2026', 4, false, true);
  expect(removed).toEqual({ text: '11.1.2026', cursor: 4 });
  const replaced = maskDateInput('11.11.2026', 5);
  expect(replaced).toEqual({ text: '11.11.2026', cursor: 6 });
  expect(parseDateInput(replaced.text)).toBe('2026-11-11');
  expect(maskDateInput('1.10.2026', 1, false, true).text).toBe('1.10.2026');
  expect(maskDateInput('11..2026', 3, false, true).text).toBe('11..2026');
  expect(maskDateInput('11.10.2026 1:30', 12, true, true).text).toBe('11.10.2026 1:30');
  expect(maskDateInput('11.10.2026 11:30', 13, true)).toEqual({
    text: '11.10.2026 11:30',
    cursor: 14,
  });
});

it.each([
  ['11.10.2026', 3, 5, '09', '11.09.2026', false],
  ['11.10.2026', 0, 2, '09', '09.10.2026', false],
  ['11.10.2026', 6, 10, '2030', '11.10.2030', false],
  ['11.10.2026 10:30', 11, 13, '09', '11.10.2026 09:30', true],
  ['11.10.2026 10:30', 14, 16, '05', '11.10.2026 10:05', true],
] as const)(
  'заменяет часть даты посимвольно: %s → %s',
  (initial, start, end, digits, expected, withTime) => {
    let edit = maskDateInput(initial.slice(0, start) + initial.slice(end), start, withTime, true);
    for (const digit of digits) {
      const next = edit.text.slice(0, edit.cursor) + digit + edit.text.slice(edit.cursor);
      edit = maskDateInput(next, edit.cursor + 1, withTime);
    }
    expect(edit.text).toBe(expected);
    expect(parseDateInput(edit.text, withTime)).not.toBeUndefined();
  },
);
