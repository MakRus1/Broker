import { describe, expect, it } from 'vitest';
import { decode, encode, parameters, same } from './catalog';
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
});
