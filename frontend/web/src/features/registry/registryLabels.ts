import type { RegistryField, RegistryFieldMetadata } from './registryFilters';
import { translate, type Locale, type MessageKey } from '../../../../shared/i18n/catalog';

const fieldKeys: Readonly<Partial<Record<string, MessageKey>>> = {
  manager_subject: 'sales.manager',
  stage_id: 'sales.stage',
  source: 'sales.source',
  next_action: 'sales.nextAction',
  next_action_date: 'sales.nextActionDate',
  ready_date: 'sales.readyDate',
  desired_delivery_date: 'sales.deliveryDate',
  payment_delay_days: 'registryField.paymentDelay',

  vat: 'registryField.vat_minor',
  quantity: 'registryField.quantityPrice',

  counterparty: 'registryField.counterparty_id',
  total: 'registryField.amount',

  route: 'registryField.route',
  mode: 'registryField.mode',
  origin_station: 'registryField.origin_station',
  destination_station: 'registryField.destination_station',
  pol: 'registryField.pol',
  pod: 'registryField.pod',
  origin_airport: 'registryField.origin_airport',
  destination_airport: 'registryField.destination_airport',

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

const scopedFieldKeys: Readonly<Record<string, Readonly<Partial<Record<string, MessageKey>>>>> = {
  shipments: { number: 'registryField.shipmentNumber' },
  contracts: { number: 'registryField.contractNumber' },
  payments: { number: 'registryField.paymentNumber' },
  invoices: { number: 'registryField.invoiceNumber', total_minor: 'registryField.totalWithVat' },
};

export function registryFieldLabel(
  id: string,
  locale: Locale,
  registry?: string,
): string | undefined {
  const key = (registry ? scopedFieldKeys[registry]?.[id] : undefined) ?? fieldKeys[id];
  return key ? translate(locale, key) : undefined;
}

export function localizeRegistryFields(
  fields: readonly RegistryFieldMetadata[],
  locale: Locale,
  registry?: string,
): RegistryField[] {
  return fields.map((field) => ({
    ...field,
    label: registryFieldLabel(field.id, locale, registry) ?? field.id,
  }));
}

const valueKeys: Readonly<Record<string, Readonly<Record<string, MessageKey>>>> = {
  status: {
    draft: 'sales.status.draft',
    in_progress: 'sales.status.in_progress',
    agreed: 'sales.status.agreed',
    rejected: 'sales.status.rejected',
  },
  direction: {
    income: 'registryValue.direction.income',
    expense: 'registryValue.direction.expense',
    both: 'registryValue.direction.both',
  },
  unit: {
    container: 'registryValue.unit.container',
    transportation: 'registryValue.unit.transportation',
    ton: 'registryValue.unit.ton',
    day: 'registryValue.unit.day',
    piece: 'registryValue.unit.piece',
  },
  payment_status: {
    unpaid: 'registryValue.payment_status.unpaid',
    partial: 'registryValue.payment_status.partial',
    paid: 'registryValue.payment_status.paid',
  },
  vat_mode: { exempt: 'registryValue.vat_mode.exempt', rate: 'registryValue.vat_mode.rate' },

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
