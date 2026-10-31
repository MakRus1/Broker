import { expect, it } from 'vitest';
import { historyValue } from './historyTypes';
it('форматирует денежные значения истории без потери точности', () => {
  expect(
    historyValue({ format: 'money', value: '9007199254740991', currency: 'RUB' }).replace(
      /\s/g,
      ' ',
    ),
  ).toBe('90 071 992 547 409,91 RUB');
  expect(historyValue({ format: 'quantity', value: '1234' })).toBe('1,234');
  expect(historyValue({ format: 'percent', value: '2000' })).toBe('20%');
  expect(historyValue({ format: 'rate', value: '91234567' })).toBe('91,234567');
});
it('различает отсутствующее значение, ноль и календарную дату', () => {
  expect(historyValue()).toBe('—');
  expect(historyValue({ format: 'money', value: '0', currency: 'RUB' })).toBe('0,00 RUB');
  expect(historyValue({ format: 'date', value: '2026-10-31' })).toBe('31.10.2026');
  expect(historyValue({ format: 'text', value: 'Нет' })).toBe('Нет');
});
