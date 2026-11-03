import { renderToStaticMarkup } from 'react-dom/server';
import { expect, it, vi } from 'vitest';
import { translate } from '../../../../../shared/i18n/catalog';
import type { Session } from '../../../shared/api/session';
import { ServiceChargeEditor, newCharge } from './ServiceChargeEditor';
import { VatRateSelect } from './VatRateSelect';
import { ServiceInvoiceActions } from '../invoices/ServiceInvoiceActions';

vi.mock('../../localization/Localization', () => ({
  useLocalization: () => ({
    locale: 'en',
    translate: (key: Parameters<typeof translate>[1], values?: Record<string, string | number>) =>
      translate('en', key, values),
  }),
}));
const session: Session = {
  actor: 'test',
  subject: 'test',
  organization_id: 'org',
  display_name: 'Test',
  csrf_token: '',
  permissions: [],
};

it('renders an English service charge form while preserving the supplied label', () => {
  const charge = {
    ...newCharge({ shipment_id: 'shipment', transportation_id: null }, 'income'),
    label_name: 'Морской фрахт клиента',
  };
  const html = renderToStaticMarkup(
    <ServiceChargeEditor
      initial={charge}
      session={session}
      customers={[]}
      onSaved={() => {}}
      onCancel={() => {}}
    />,
  );
  expect(html).toContain('Financial label · required');
  expect(html).toContain('Service date · required');
  expect(html).toContain('Quantity · required');
  expect(html).toContain('Service currency');
  expect(html).toContain('Russian');
  expect(html).toContain('Excl. VAT');
  expect(html).toContain('VAT exempt');
  expect(html).toContain('Морской фрахт клиента');
  expect(html).not.toContain('Количество · обязательно');
});
it('renders custom VAT controls in English without replacing the rate', () => {
  const html = renderToStaticMarkup(<VatRateSelect value="7,25" onChange={() => {}} />);
  expect(html).toContain('7,25');
  expect(html).toContain('VAT rate');
  expect(html).not.toContain('Другая ставка');
});
it('renders a linked invoice number and date as one localized message', () => {
  const current = { invoice_id: 'invoice', number: 'ТЕСТ-123', invoice_date: '2026-10-30' };
  const service = {
    direction: 'income' as const,
    counterparty_id: 'customer',
    counterparty_name: 'Клиент',
    contract_id: null,
    contract_number: null,
    invoice_currency: 'RUB',
  };
  const html = renderToStaticMarkup(
    <ServiceInvoiceActions
      service={service}
      current={current}
      session={session}
      onSelect={async () => {}}
    />,
  );
  expect(html).toContain('#ТЕСТ-123 dated 30.10.2026');
  expect(html).toContain('Unlink');
});
