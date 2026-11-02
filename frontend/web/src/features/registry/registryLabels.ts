import { translate, type Locale, type MessageKey } from '../../../../shared/i18n/catalog';

const fieldKeys: Readonly<Record<string, MessageKey>> = {
  own_company_id: 'registryField.own_company_id',
  counterparty_id: 'registryField.counterparty_id',
  payment_date: 'registryField.payment_date',
  currency: 'registryField.currency',
  has_unallocated: 'registryField.has_unallocated',
  label_name: 'registryField.label_name',
  direction: 'registryField.direction',
  service_date: 'registryField.service_date',
  invoice_number: 'registryField.invoice_number',
  transportation_id: 'registryField.transportation_id',
  net_minor: 'registryField.net_minor',
  vat_minor: 'registryField.vat_minor',
  total_minor: 'registryField.total_minor',
  contract_date: 'registryField.contract_date',
  valid_until: 'registryField.valid_until',
  invoice_date: 'registryField.invoice_date',
  due_date: 'registryField.due_date',
  contract_id: 'registryField.contract_id',
  paid_minor: 'registryField.paid_minor',
  outstanding_minor: 'registryField.outstanding_minor',
  payment_status: 'registryField.payment_status',
  overdue_days: 'registryField.overdue_days',
  overdue: 'registryField.overdue',
  unit: 'registryField.unit',
  vat_mode: 'registryField.vat_mode',

  number: 'registryField.number',
  id: 'registryField.id',
  shipment_date: 'registryField.shipment_date',
  cargo_description: 'registryField.cargo_description',
  customer_id: 'registryField.customer_id',
  status: 'registryField.status',
  container_number: 'registryField.container_number',
  type: 'registryField.type',
  container_type: 'registryField.container_type',
  container_ownership: 'registryField.container_ownership',
  hazardous: 'registryField.hazardous',
  shipment_id: 'registryField.shipment_id',
  item_id: 'registryField.item_id',
  incomplete_container: 'registryField.incomplete_container',
  name: 'registryField.name',
  country_code: 'registryField.country_code',
  tax_id: 'registryField.tax_id',
  archived: 'registryField.archived',
  own_company: 'registryField.own_company',
  roles: 'registryField.roles',
  search: 'registryField.search',
};

// API identifiers remain stable; unknown fields retain the server's label.
export function registryFieldLabel(id: string, fallback: string, locale: Locale): string {
  if (locale === 'ru') return fallback;
  const key = fieldKeys[id];
  return key ? translate(locale, key) : fallback;
}

const valueKeys: Readonly<Record<string, Readonly<Record<string, MessageKey>>>> = {
  type: {
    consolidated: 'registryValue.type.consolidated',
    container: 'registryValue.type.container',
    air: 'registryValue.type.air',
    oversized: 'registryValue.type.oversized',
    rail: 'registryValue.type.rail',
    vessel: 'registryValue.type.vessel',
  },
  mode: {
    sea: 'registryValue.mode.sea',
    international_rail: 'registryValue.mode.international_rail',
    international_road: 'registryValue.mode.international_road',
    rail: 'registryValue.mode.rail',
    road: 'registryValue.mode.road',
    pickup: 'registryValue.mode.pickup',
    air: 'registryValue.mode.air',
  },
  roles: {
    customer: 'registryValue.roles.customer',
    carrier: 'registryValue.roles.carrier',
    agent: 'registryValue.roles.agent',
    supplier: 'registryValue.roles.supplier',
    partner: 'registryValue.roles.partner',
  },
};
export function registryValueLabel(
  field: string,
  value: string,
  locale: Locale,
): string | undefined {
  const key = valueKeys[field]?.[value];
  return key ? translate(locale, key) : undefined;
}
