import type { Locale } from "./i18n/catalog";

// Currency codes and localized names come from the runtime's Intl (CLDR) data.
const preferred = ["RUB", "USD", "EUR", "CNY", "AED"];
const names = {
  ru: new Intl.DisplayNames(["ru"], { type: "currency" }),
  en: new Intl.DisplayNames(["en"], { type: "currency" }),
};
function buildOptions(locale: Locale) {
  return Intl.supportedValuesOf("currency")
    .map((value) => ({
      value,
      label: `${value} · ${names[locale].of(value) ?? value}`,
    }))
    .sort((a, b) => {
      const rank = (code: string) =>
        preferred.includes(code) ? preferred.indexOf(code) : preferred.length;
      return (
        rank(a.value) - rank(b.value) || a.label.localeCompare(b.label, locale)
      );
    });
}
const options = { ru: buildOptions("ru"), en: buildOptions("en") };
export const currencyOptions = options.ru;
export function currenciesWithCurrent(value: string, locale: Locale = "ru") {
  const choices = options[locale];
  return choices.some((item) => item.value === value) || !value
    ? choices
    : [
        ...choices,
        { value, label: `${value} · ${names[locale].of(value) ?? value}` },
      ];
}
