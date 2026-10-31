import { beforeEach, expect, it, vi } from 'vitest';
import { renderToStaticMarkup } from 'react-dom/server';
import { request, type Session } from './api';
import {
  attachServiceInvoice,
  ServiceInvoiceActions,
  type ServiceInvoiceSelection,
} from './ServiceInvoiceActions';
import type { Invoice, InvoiceDetails } from './invoices';
vi.mock('./api', async (importOriginal) => ({
  ...(await importOriginal<typeof import('./api')>()),
  request: vi.fn(),
}));
const session = { permissions: ['finance.write_invoices'] } as Session;
const invoice: Invoice = {
  id: 'invoice',
  direction: 'outgoing',
  number: 'ИС-7',
  invoice_date: '2026-10-30',
  due_date: null,
  own_company_id: 'own',
  own_company_name: 'Своя компания',
  counterparty_id: 'party',
  counterparty_name: 'Клиент',
  own_bank_account_id: null,
  counterparty_bank_account_id: null,
  contract_id: null,
  contract_number: null,
  currency: 'RUB',
  note: null,
  archived: false,
  revision: 3,
  net_minor: 10000,
  vat_minor: 2000,
  total_minor: 12000,
};
const selected: ServiceInvoiceSelection = { kind: 'existing', invoice };
const saved = { invoice, charges: [{ id: 'service' }] } as InvoiceDetails;
beforeEach(() => vi.mocked(request).mockReset());
it('включает существующую услугу по текущей редакции счёта', async () => {
  vi.mocked(request).mockResolvedValue(saved);
  expect(await attachServiceInvoice('service', selected, session)).toBe(saved);
  expect(request).toHaveBeenCalledExactlyOnceWith('/api/v1/invoices/add-service-charge', session, {
    invoice_id: 'invoice',
    service_charge_id: 'service',
    expected_revision: 3,
  });
});
it('создаёт счёт сразу с услугой', async () => {
  const {
    own_company_name,
    counterparty_name,
    contract_number,
    revision,
    net_minor,
    vat_minor,
    total_minor,
    ...body
  } = invoice;
  vi.mocked(request).mockResolvedValue(saved);
  await attachServiceInvoice('service', { kind: 'new', body }, session);
  expect(request).toHaveBeenCalledExactlyOnceWith('/api/v1/invoices/save', session, {
    ...body,
    service_charge_ids: ['service'],
    expected_revision: 0,
  });
});
it('распознаёт успешное включение при потерянном ответе', async () => {
  vi.mocked(request)
    .mockRejectedValueOnce(new Error('Соединение потеряно'))
    .mockResolvedValueOnce(saved);
  expect(await attachServiceInvoice('service', selected, session)).toBe(saved);
  expect(request).toHaveBeenNthCalledWith(2, '/api/v1/invoices/get', session, { id: 'invoice' });
});
it('сохраняет ошибку, если услуга не оказалась в счёте', async () => {
  const error = new Error('Счёт изменён');
  vi.mocked(request)
    .mockRejectedValueOnce(error)
    .mockResolvedValueOnce({ ...saved, charges: [] });
  await expect(attachServiceInvoice('service', selected, session)).rejects.toBe(error);
});
it('показывает номер и дату выбранного счёта прямо в поле услуги', () => {
  const html = renderToStaticMarkup(
    <ServiceInvoiceActions
      service={{
        direction: 'income',
        counterparty_id: 'party',
        counterparty_name: 'Клиент',
        contract_id: null,
        contract_number: null,
        invoice_currency: 'RUB',
      }}
      session={session}
      selection={selected}
      onSelect={() => {}}
    />,
  );
  expect(html).toContain('ИС-7');
  expect(html).toContain('30.10.2026');
  expect(html).not.toContain('href=');
});
it('не предлагает уходить из услуги в другой раздел для создания счёта', () => {
  const html = renderToStaticMarkup(
    <ServiceInvoiceActions
      service={{
        direction: 'income',
        counterparty_id: 'party',
        counterparty_name: 'Клиент',
        contract_id: null,
        contract_number: null,
        invoice_currency: 'RUB',
      }}
      session={session}
      onSelect={() => {}}
    />,
  );
  expect(html).toContain('Выберите или создайте счёт');
  expect(html).not.toContain('Не выбран');
  expect(html).not.toContain('href=');
});
it('переносит услугу с проверкой исходного счёта', async () => {
  vi.mocked(request).mockResolvedValue(saved);
  await attachServiceInvoice('service', selected, session, 'old-invoice');
  expect(request).toHaveBeenCalledExactlyOnceWith('/api/v1/invoices/add-service-charge', session, {
    invoice_id: 'invoice',
    service_charge_id: 'service',
    expected_revision: 3,
    previous_invoice_id: 'old-invoice',
  });
});
it('показывает отвязку и единое поле для уже связанной услуги', () => {
  const html = renderToStaticMarkup(
    <ServiceInvoiceActions
      service={{
        direction: 'income',
        counterparty_id: 'party',
        counterparty_name: 'Клиент',
        contract_id: null,
        contract_number: null,
        invoice_currency: 'RUB',
      }}
      session={session}
      current={{ invoice_id: 'invoice', number: 'ИС-7', invoice_date: '2026-10-30' }}
      onSelect={() => {}}
    />,
  );
  expect(html).toContain('Отвязать');
  expect(html).toContain('ИС-7');
  expect(html).toContain('30.10.2026');
});
