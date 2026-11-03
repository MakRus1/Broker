import { translate, type MessageKey } from '../../../../shared/i18n/catalog';

// Non-React helpers use the language applied by Localization to the document.
export function interfaceLocale() {
  return typeof document !== 'undefined' && document.documentElement.lang === 'en' ? 'en' : 'ru';
}
export function interfaceText(key: MessageKey, values?: Record<string, string | number>) {
  return translate(interfaceLocale(), key, values);
}
