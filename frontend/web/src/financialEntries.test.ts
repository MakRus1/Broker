import { describe, expect, it } from 'vitest';
import { parseFixed, formatFixed, inputFixed } from './financialEntries';
describe('Точные денежные значения', () => {
  it('принимает точку и запятую без float', () => {
    expect(parseFixed('12,34', 2, 100000000)).toBe(1234);
    expect(parseFixed('1.005', 3, 10000000)).toBe(1005);
    for (const text of ['-1', '1e3', '12.345', 'abc', '999999999999999999999999'])
      expect(parseFixed(text, 2, 100000000)).toBeUndefined();
  });
  it('форматирует большие сводки и отрицательную разницу без потери точности', () => {
    expect(formatFixed('9007199254740993', 2)).toContain(',93');
    expect(formatFixed(-123n, 2)).toBe('−1,23');
    expect(parseFixed(inputFixed(12345, 3), 3, 10000000)).toBe(12345);
  });
});
