import { describe, expect, it } from 'vitest';
import { builtins, decode, encode, parameters, same } from './catalog';
describe('translation drafts', () => {
  it('keeps both locales and removes empty overrides', () => {
    const catalog = { ru: [{ key: 'nav.shipments', value: 'Букинги' }], en: [] };
    const values = decode(catalog);
    values.en['nav.shipments'] = '';
    expect(encode(values)).toEqual(catalog);
    expect(same(values, decode(catalog))).toBe(true);
  });
  it('checks parameters as names independent of order and repetition', () => {
    expect(parameters('{count} {name} {count}')).toBe(parameters('{name} {count}'));
    expect(parameters('{name}')).not.toBe(parameters('{count}'));
  });
  it('uses the same built-in key set in both languages', () => {
    expect(Object.keys(builtins.ru).sort()).toEqual(Object.keys(builtins.en).sort());
    for (const key of Object.keys(builtins.ru) as (keyof typeof builtins.ru)[]) {
      expect(parameters(builtins.ru[key]), key).toBe(parameters(builtins.en[key]));
    }
  });
});
