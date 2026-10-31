import { it, expect } from 'vitest';
import { normalizeDecimalInput } from './DecimalInput';
it('сохраняет редактирование десятичного числа и принимает вставку с пробелами', () => {
  for (const value of ['', '0', '12,', '12.3', '12,34'])
    expect(normalizeDecimalInput(value, 2)).toBe(value);
  expect(normalizeDecimalInput('1 234,50', 2)).toBe('1234,50');
  expect(normalizeDecimalInput('1\u202f234.50', 2)).toBe('1234.50');
  for (const value of ['abc', '1e3', '-1', '12,345', '1.2.3', '1,2.3'])
    expect(normalizeDecimalInput(value, 2)).toBeUndefined();
  expect(normalizeDecimalInput('1,234', 3)).toBe('1,234');
});
