// Коды и локализованные названия из справочника валют среды Intl (CLDR).
const names = new Intl.DisplayNames(['ru'], { type: 'currency' });
const preferred = ['RUB', 'USD', 'EUR', 'CNY', 'AED'];
export const currencyOptions = Intl.supportedValuesOf('currency')
  .map((value) => ({ value, label: `${value} · ${names.of(value) ?? value}` }))
  .sort((a, b) => {
    const rank = (code: string) =>
      preferred.includes(code) ? preferred.indexOf(code) : preferred.length;
    return rank(a.value) - rank(b.value) || a.label.localeCompare(b.label, 'ru');
  });
export function currenciesWithCurrent(value: string) {
  return currencyOptions.some((item) => item.value === value) || !value
    ? currencyOptions
    : [...currencyOptions, { value, label: `${value} · ${names.of(value) ?? value}` }];
}
