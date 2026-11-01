import { beforeEach, expect, it, vi } from 'vitest';
import {
  automaticInvoiceAmounts,
  invoicePaymentAdditions,
  invoicePaymentProblem,
  saveInvoicePayments,
  type PaymentTarget,
} from './invoicePayments';
import { request } from '../../../shared/api/http';
import type { PaymentDetails } from '../payments/payments';
import type { Session } from '../../../shared/api/session';
vi.mock('../../../shared/api/http', () => ({ request: vi.fn() }));
const first = {
  invoice: {
    id: 'a',
    number: 'A',
    direction: 'outgoing',
    own_company_id: 'own',
    counterparty_id: 'party',
    currency: 'RUB',
    archived: false,
  },
  outstanding_minor: 12000,
} as PaymentTarget;
const second = { ...first, invoice: { ...first.invoice, id: 'b', number: 'B' } };
beforeEach(() => vi.mocked(request).mockReset());
it('распределяет доступный остаток по порядку, включая частичную оплату', () => {
  expect(automaticInvoiceAmounts([first, second], 15000)).toEqual({ a: '120,00', b: '30,00' });
  expect(automaticInvoiceAmounts([first, second], 5000)).toEqual({ a: '50,00', b: '0,00' });
});
it('не объединяет несовместимые стороны, валюты и направления', () => {
  expect(invoicePaymentProblem([first, second])).toBeUndefined();
  for (const change of [
    { own_company_id: 'other' },
    { counterparty_id: 'other' },
    { currency: 'USD' },
    { direction: 'incoming' },
    { archived: true },
  ])
    expect(
      invoicePaymentProblem([
        first,
        { ...second, invoice: { ...second.invoice, ...change } } as PaymentTarget,
      ]),
    ).toBeTruthy();
  expect(invoicePaymentProblem([{ ...first, outstanding_minor: 0 }])).toBeTruthy();
});
it('разрешает ноль для отдельного счёта и запрещает превышения и пустой ввод', () => {
  expect(invoicePaymentAdditions([first, second], { a: '0', b: '30' }, 5000)).toEqual([
    { invoice_id: 'b', amount_minor: 3000 },
  ]);
  for (const amounts of [
    { a: '0', b: '0' },
    { a: '', b: '1' },
    { a: '121', b: '0' },
    { a: '50', b: '50' },
  ])
    expect(() => invoicePaymentAdditions([first, second], amounts, 5000)).toThrow();
});
it('сохраняет прежние распределения, добавляет новые одним запросом с исходной ревизией', async () => {
  const details = {
    payment: { id: 'payment', revision: 3 },
    allocations: [
      { invoice_id: 'old', amount_minor: 500 },
      { invoice_id: 'a', amount_minor: 1000 },
    ],
  } as PaymentDetails;
  await saveInvoicePayments(
    details,
    [
      { invoice_id: 'a', amount_minor: 2000 },
      { invoice_id: 'b', amount_minor: 3000 },
    ],
    {} as Session,
  );
  expect(request).toHaveBeenCalledExactlyOnceWith(
    '/api/v1/payments/save',
    {},
    expect.objectContaining({
      expected_revision: 3,
      allocations: [
        { invoice_id: 'old', amount_minor: 500 },
        { invoice_id: 'a', amount_minor: 3000 },
        { invoice_id: 'b', amount_minor: 3000 },
      ],
    }),
  );
  expect(details.allocations[1].amount_minor).toBe(1000);
});
it('не повторяет сохранение после конфликта редакции', async () => {
  vi.mocked(request).mockRejectedValueOnce(new Error('Платёж изменён'));
  const details = {
    payment: { id: 'payment', revision: 3 },
    allocations: [],
  } as unknown as PaymentDetails;
  await expect(
    saveInvoicePayments(details, [{ invoice_id: 'a', amount_minor: 100 }], {} as Session),
  ).rejects.toThrow('Платёж изменён');
  expect(request).toHaveBeenCalledTimes(1);
});

it('не отправляет запрос при превышении лимита распределений с учётом уже существующих', async () => {
  const details = {
    payment: { id: 'payment', revision: 3 },
    allocations: Array.from({ length: 100 }, (_, index) => ({
      invoice_id: `old-${index}`,
      amount_minor: 1,
    })),
  } as unknown as PaymentDetails;
  await expect(
    saveInvoicePayments(details, [{ invoice_id: 'new', amount_minor: 100 }], {} as Session),
  ).rejects.toThrow('100');
  expect(request).not.toHaveBeenCalled();
});
it('отсекает больше 100 выбранных счетов до открытия распределения', () => {
  expect(
    invoicePaymentProblem(
      Array.from({ length: 101 }, (_, index) => ({
        ...first,
        invoice: { ...first.invoice, id: String(index) },
      })),
    ),
  ).toContain('100');
});
