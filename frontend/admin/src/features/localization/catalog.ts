export type Locale = 'ru' | 'en';
export type Messages = Record<string, string>;
export type Catalog = Record<Locale, { key: string; value: string }[]>;
export type Values = Record<Locale, Messages>;
export interface EditorState {
  version: number;
  revision: number;
  published: Catalog;
  inherited: Catalog;
  source: Catalog;
  draft?: Catalog;
}
export function decode(catalog: Catalog): Values {
  return {
    ru: Object.fromEntries(catalog.ru.map((x) => [x.key, x.value])),
    en: Object.fromEntries(catalog.en.map((x) => [x.key, x.value])),
  };
}
export function encode(values: Values): Catalog {
  const entries = (locale: Locale) =>
    Object.entries(values[locale])
      .filter(([, value]) => value !== '')
      .sort(([a], [b]) => a.localeCompare(b))
      .map(([key, value]) => ({ key, value }));
  return { ru: entries('ru'), en: entries('en') };
}
export function same(a: Values, b: Values): boolean {
  return JSON.stringify(encode(a)) === JSON.stringify(encode(b));
}
export function parameters(value: string): string {
  return [...new Set(value.match(/\{\w+\}/g) ?? [])].sort().join(', ');
}
