import { afterEach, expect, it } from 'vitest';
import {
  clearPublishedMessages,
  replacePublishedMessages,
  translate,
  validatePublishedMessages,
} from '../../../../shared/i18n/catalog';

afterEach(clearPublishedMessages);

it('applies a published translation and preserves interpolation', () => {
  replacePublishedMessages(
    'en',
    validatePublishedMessages(
      [
        { key: 'common.loaded', value: '{count} records loaded' },
        { key: 'nav.shipments', value: 'Bookings' },
      ],
      'en',
    ),
  );
  expect(translate('en', 'nav.shipments')).toBe('Bookings');
  expect(translate('en', 'common.loaded', { count: 12 })).toBe('12 records loaded');
  expect(translate('ru', 'nav.shipments')).toBe('Поставки');
});

it('falls back for unknown keys, malformed translations and mismatched placeholders', () => {
  const messages = validatePublishedMessages(
    [
      { key: 'common.loaded', value: 'Lost count' },
      { key: 'action.refresh', value: '' },
      { key: 'nav.shipments', value: 123 },
      { key: '__proto__', value: 'unsafe' },
      { key: 'missing.key', value: 'unknown' },
    ],
    'en',
  );
  expect(messages).toEqual({});
  replacePublishedMessages('en', messages);
  expect(translate('en', 'common.loaded', { count: 12 })).toBe('Loaded: 12');
});

it('drops the previous catalog when switching scope', () => {
  replacePublishedMessages('en', { 'nav.shipments': 'Bookings' });
  clearPublishedMessages();
  expect(translate('en', 'nav.shipments')).toBe('Shipments');
});
