import { renderToStaticMarkup } from 'react-dom/server';
import { expect, it, vi } from 'vitest';
import { translate } from '../../../../../shared/i18n/catalog';
import { ServiceChargeSummary } from './ServiceChargeSummary';
import type { ServiceCharge } from './serviceCharges';

vi.mock('../../localization/Localization', () => ({
  useLocalization: () => ({
    locale: 'en',
    translate: (key: Parameters<typeof translate>[1], values?: Record<string, string | number>) =>
      translate('en', key, values),
  }),
}));
const charge: ServiceCharge = {
  id: 'charge',
  shipment_id: 'shipment',
  transportation_id: null,
  label_id: 'label',
  label_name: 'Freight',
  direction: 'income',
  unit: 'transportation',
  counterparty_id: 'customer',
  counterparty_name: 'Customer',
  contract_id: 'contract',
  contract_number: 'C-1',
  quantity_milli: 1000,
  price_minor: 10000,
  price_mode: 'net',
  currency: 'USD',
  service_date: '2026-11-03',
  invoice_currency: 'RUB',
  invoice_rate: { date: '2026-11-03', rate_millionths: 90000000, source: 'manual' },
  invoice_net_minor: 900000,
  invoice_vat_minor: 180000,
  invoice_total_minor: 1080000,
  vat_mode: 'rate',
  vat_rate_basis_points: 2000,
  net_minor: 10000,
  vat_minor: 2000,
  total_minor: 12000,
  note: null,
  archived: false,
  revision: 1,
};
it('shows stored service and invoice amounts separately without recalculation', () => {
  const html = renderToStaticMarkup(<ServiceChargeSummary charge={charge} />);
  expect(html).toContain('120,00');
  expect(html).toMatch(/10\s800,00/);
  expect(html).toContain('USD');
  expect(html).toContain('RUB');
  expect(html).toContain('90,000000');
  expect(html).not.toContain('<input');
  expect(html).not.toContain('<form');
});
it('links the exact invoice and distinguishes exempt VAT from a zero rate', () => {
  const html = renderToStaticMarkup(
    <ServiceChargeSummary
      charge={{ ...charge, vat_mode: 'exempt', vat_rate_basis_points: null }}
      invoice={{
        invoice_id: 'invoice',
        number: 'INV-1',
        invoice_date: '2026-11-03',
        direction: 'outgoing',
        service_charge_id: 'charge',
        read_only: true,
      }}
    />,
  );
  expect(html).toContain('#/outgoing-invoices/invoice');
  expect(html).toContain('INV-1');
  expect(html).toContain('03.11.2026');
  expect(html).toContain('VAT exempt');
  expect(html).toContain('#/contracts/contract');
});
