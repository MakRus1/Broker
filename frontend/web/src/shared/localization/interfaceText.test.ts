import { afterEach, expect, it, vi } from 'vitest';
import { interfaceText } from './interfaceText';

afterEach(() => vi.unstubAllGlobals());
it('uses the active interface language without caching a previous selection', () => {
  const document = { documentElement: { lang: 'en' } };
  vi.stubGlobal('document', document);
  expect(interfaceText('errors.request')).toBe('Could not complete the request. Please try again.');
  document.documentElement.lang = 'ru';
  expect(interfaceText('errors.trace', { id: 'abc' })).toBe('Трейс: abc');
});
it('keeps Russian as the fallback outside the browser', () => {
  expect(interfaceText('validation.required')).toBe('Заполните обязательное поле');
});
