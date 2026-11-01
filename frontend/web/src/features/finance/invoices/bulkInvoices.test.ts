import { beforeEach, expect, it, vi } from 'vitest';
import { request } from '../../../shared/api/http';
import type { Session } from '../../../shared/api/session';
import { attachBulkInvoice, bulkInvoiceProblem, type SelectableCharge } from './bulkInvoices';
import type { Invoice } from './invoices';
vi.mock('../../../shared/api/http', () => ({ request: vi.fn() }));
const session = {} as Session;
const first = {
  id: 'a',
  direction: 'income',
  counterparty_id: 'party',
  contract_id: null,
  invoice_currency: 'RUB',
  archived: false,
} as SelectableCharge;
const invoice = { id: 'invoice', revision: 3, archived: false } as Invoice;
beforeEach(() => vi.mocked(request).mockReset());
it('совместимость учитывает направление, контрагента, договор и валюту счёта', () => {
  expect(bulkInvoiceProblem([first, { ...first, id: 'b' }])).toBeUndefined();
  for (const change of [
    { direction: 'expense' },
    { counterparty_id: 'other' },
    { contract_id: 'contract' },
    { invoice_currency: 'USD' },
    { archived: true },
    { invoice: {} },
  ])
    expect(bulkInvoiceProblem([first, { ...first, ...change } as SelectableCharge])).toBeTruthy();
});
it('добавляет все услуги одним save и сохраняет существующие строки', async () => {
  vi.mocked(request)
    .mockResolvedValueOnce({ invoice, charges: [{ id: 'old' }] })
    .mockResolvedValueOnce({ invoice, charges: [] });
  await attachBulkInvoice(['a', 'b'], { kind: 'existing', invoice }, session);
  expect(request).toHaveBeenNthCalledWith(
    2,
    '/api/v1/invoices/save',
    session,
    expect.objectContaining({ service_charge_ids: ['old', 'a', 'b'], expected_revision: 3 }),
  );
  expect(request).toHaveBeenCalledTimes(2);
});
it('не перезаписывает счёт, изменившийся после выбора', async () => {
  vi.mocked(request).mockResolvedValueOnce({ invoice: { ...invoice, revision: 4 }, charges: [] });
  await expect(attachBulkInvoice(['a'], { kind: 'existing', invoice }, session)).rejects.toThrow(
    'Счёт изменён',
  );
  expect(request).toHaveBeenCalledTimes(1);
});
it('потерянный ответ признаётся успешным только когда включены все выбранные услуги', async () => {
  const failure = new Error('network');
  vi.mocked(request)
    .mockResolvedValueOnce({ invoice, charges: [] })
    .mockRejectedValueOnce(failure)
    .mockResolvedValueOnce({ invoice, charges: [{ id: 'a' }] });
  await expect(attachBulkInvoice(['a', 'b'], { kind: 'existing', invoice }, session)).rejects.toBe(
    failure,
  );
});
