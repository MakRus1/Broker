import { describe, it, expect } from 'vitest';
import { parseVatRate, formatVatRate, vatLabel } from './financialLabels';
describe('НДС финансовых меток', () => {
  it('переводит десятичную ставку точно в сотые процента', () => {
    expect(parseVatRate('22,5')).toBe(2250);
    expect(parseVatRate(' 0.01 ')).toBe(1);
    expect(parseVatRate('100')).toBe(10000);
    for (let value = 0; value <= 10000; value++)
      expect(parseVatRate(formatVatRate(value))).toBe(value);
  });
  it('отклоняет неверные и слишком точные ставки', () => {
    for (const value of ['', '-1', '100,01', '1.234', '1e1', 'NaN', 'Infinity', '1,2.3'])
      expect(parseVatRate(value)).toBeUndefined();
  });
  it('различает без НДС и нулевую ставку', () => {
    expect(vatLabel({ vat_mode: 'exempt', vat_rate_basis_points: null })).toBe('Без НДС');
    expect(vatLabel({ vat_mode: 'rate', vat_rate_basis_points: 0 })).toBe('0%');
  });
});
