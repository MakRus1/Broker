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

it('translates rail and road fields without translating selected directory records', async () => {
  const { RailFields } = await import('../../routes/RailFields');
  const { RoadDetailsView } = await import('../../routes/RoadFields');
  const rail = renderToStaticMarkup(
    <RailFields
      value={{
        origin_station: {
          id: 'station',
          name: 'Владивосток',
          country_code: 'RU',
          esr_code: '981106',
        },
      }}
      session={session}
      onChange={() => {}}
    />,
  );
  expect(rail).toContain('Departure station');
  expect(rail).toContain('Train departure');
  expect(rail).toContain('Владивосток');
  expect(rail).toContain('ESR 981106');
  const road = renderToStaticMarkup(
    <RoadDetailsView pickup={{ driver: 'Иванов Иван', vehicle_number: 'А123ВС' }} customers={[]} />,
  );
  expect(road).toContain('Driver');
  expect(road).toContain('Vehicle number');
  expect(road).toContain('Иванов Иван');
});

it('translates transportation types, ownership and container warnings together', async () => {
  const { TransportationDetailsForm } =
    await import('../../transportations/TransportationDetailsForm');
  const html = renderToStaticMarkup(
    <TransportationDetailsForm
      shipmentId="shipment"
      session={session}
      onSaved={() => {}}
      onCancel={() => {}}
      onError={() => {}}
    />,
  );
  expect(html).toContain('Transportation type');
  expect(html).toContain('Container number');
  expect(html).toContain('Container ownership');
  expect(html).toContain('Create transportation');
  expect(html).not.toContain('Тип перевозки');
});

it('translates counterparty details and roles without changing the company name or tax ID', async () => {
  const { CounterpartyForm } = await import('../../counterparties/CounterpartyForm');
  const html = renderToStaticMarkup(
    <CounterpartyForm
      session={session}
      initial={{
        id: 'company',
        name: 'Мой клиент',
        legal_name: '',
        country_code: 'RU',
        tax_id: '7707083893',
        kpp: '',
        registration_id: '',
        legal_address: '',
        roles: ['customer'],
        contacts: [],
        own_company: false,
        archived: false,
        revision: 1,
      }}
      onSaved={() => {}}
      onError={() => {}}
    />,
  );
  expect(html).toContain('Edit counterparty');
  expect(html).toContain('Customer');
  expect(html).toContain('Short name · required');
  expect(html).toContain('Мой клиент');
  expect(html).toContain('7707083893');
  expect(html).not.toContain('Контактные лица');
});
