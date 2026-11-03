import { expect, it } from 'vitest';
import {
  historyValue,
  historyTitle,
  historyFieldLabel,
  historyChangeKey,
  historyReferenceIdsDiffer,
  type HistoryEvent,
} from './historyTypes';
it('форматирует денежные значения истории без потери точности', () => {
  expect(
    historyValue({ format: 'money', value: '9007199254740991', currency: 'RUB' }).replace(
      /\s/g,
      ' ',
    ),
  ).toBe('90 071 992 547 409,91 RUB');
  expect(historyValue({ format: 'quantity', value: '1234' })).toBe('1,234');
  expect(historyValue({ format: 'percent', value: '2000' })).toBe('20%');
  expect(historyValue({ format: 'rate', value: '91234567' })).toBe('91,234567');
});
it('различает отсутствующее значение, ноль и календарную дату', () => {
  expect(historyValue()).toBe('—');
  expect(historyValue({ format: 'money', value: '0', currency: 'RUB' })).toBe('0,00 RUB');
  expect(historyValue({ format: 'date', value: '2026-10-31' })).toBe('31.10.2026');
  expect(historyValue({ format: 'text', value: 'Нет' })).toBe('Нет');
});

it('translates enum and boolean codes but leaves user text unchanged', () => {
  expect(historyValue({ format: 'enum', value: 'income' }, 'en', 'direction')).toBe('Income');
  expect(historyValue({ format: 'enum', value: 'gross' }, 'en', 'price_mode')).toBe('Incl. VAT');
  expect(historyValue({ format: 'enum', value: 'exempt' }, 'en', 'vat_mode')).toBe('VAT exempt');
  expect(historyValue({ format: 'boolean', value: 'false' }, 'en')).toBe('No');
  expect(historyValue({ format: 'text', value: 'Доход от клиента' }, 'en')).toBe(
    'Доход от клиента',
  );
  expect(historyValue({ format: 'enum', value: 'new-unit' }, 'en', 'unit')).toBe('new-unit');
});

it('localizes event titles and nested changes using explicit document context', () => {
  const event: HistoryEvent = {
    actor: 'author',
    cursor: {
      source: 'invoice',
      entity_id: 'invoice',
      revision: 2,
      occurred_at: '2026-10-31T12:00:00Z',
    },
    action: 'invoice_linked',
    document: 'invoice',
    reference: { source: 'invoice', id: 'invoice', name: 'ИС-123' },
    has_previous: true,
    changes: [],
  };
  expect(historyTitle(event, 'en')).toBe('Service charge included in invoice #ИС-123');
  expect(historyTitle(event, 'ru')).toBe('Услуга включена в счёт № ИС-123');
  const change = {
    field: 'price_minor' as const,
    subject: { source: 'service_charge' as const, id: 'charge', name: 'Морской фрахт' },
  };
  expect(historyFieldLabel(change, 'invoice', 'en')).toBe('Service charge “Морской фрахт”: Price');
  expect(historyChangeKey(change)).not.toBe(
    historyChangeKey({ ...change, subject: { ...change.subject, id: 'other' } }),
  );
});
it('distinguishes references with identical names without modifying their saved values', () => {
  const change = {
    field: 'counterparty_id' as const,
    before: { format: 'reference' as const, value: 'old', display_name: 'Компания' },
    after: { format: 'reference' as const, value: 'new', display_name: 'Компания' },
  };
  expect(historyReferenceIdsDiffer(change)).toBe(true);
  expect(historyValue(change.before, 'en', change.field, true)).toBe('Компания (old)');
  expect(historyValue(change.after, 'en', change.field, true)).toBe('Компания (new)');
  expect(historyValue({ format: 'reference', value: 'unknown' }, 'en')).toBe('unknown');
});

it('uses document labels for ordinary invoice and payment revisions', () => {
  const event: HistoryEvent = {
    actor: 'author',
    cursor: {
      source: 'invoice',
      entity_id: 'invoice',
      revision: 1,
      occurred_at: '2026-10-31T12:00:00Z',
    },
    action: 'created',
    document: 'invoice',
    has_previous: false,
    changes: [],
  };
  expect(historyTitle(event, 'en')).toBe('Invoice: created');
  expect(historyTitle({ ...event, document: 'disbursement', action: 'restored' }, 'en')).toBe(
    'Disbursement: restored',
  );
});
