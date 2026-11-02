import { describe, expect, it } from 'vitest';
import { restoreTheme, resolveTheme } from './theme';

describe('тема оформления', () => {
  it('использует системную тему для отсутствующей или неизвестной настройки', () => {
    for (const value of [null, undefined, '', 'neon', {}, false])
      expect(restoreTheme(value)).toBe('system');
    expect(restoreTheme('dark')).toBe('dark');
    expect(restoreTheme('light')).toBe('light');
  });
  it('явный выбор важнее настройки системы', () => {
    expect(resolveTheme('dark', false)).toBe('dark');
    expect(resolveTheme('light', true)).toBe('light');
    expect(resolveTheme('system', true)).toBe('dark');
    expect(resolveTheme('system', false)).toBe('light');
  });
});
