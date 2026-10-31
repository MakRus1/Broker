import { expect, it } from 'vitest';
import { currencyOptions, currenciesWithCurrent } from './currencies';

it('показывает основные валюты первыми с русскими названиями', () => {
  expect(currencyOptions.slice(0, 5).map((item) => item.value)).toEqual([
    'RUB',
    'USD',
    'EUR',
    'CNY',
    'AED',
  ]);
  expect(currencyOptions[0].label).toContain('рубль');
  expect(new Set(currencyOptions.map((item) => item.value)).size).toBe(currencyOptions.length);
});
it('сохраняет доступ к существующему коду вне списка браузера', () => {
  expect(currenciesWithCurrent('XYZ').some((item) => item.value === 'XYZ')).toBe(true);
  expect(currenciesWithCurrent('RUB')).toBe(currencyOptions);
});
