import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { translate, type Locale, type MessageKey } from '../../../../../shared/i18n/catalog';
import { registryValueLabel } from '../../registry/registryLabels';
import { formatFixed } from '../services/serviceCharges';
export type HistoryKind = 'service-charges' | 'invoices' | 'payments';
export type HistoryAction =
  | 'created'
  | 'updated'
  | 'archived'
  | 'restored'
  | 'invoice_linked'
  | 'invoice_unlinked'
  | 'payment_allocated'
  | 'payment_reallocated'
  | 'payment_unallocated';
export type HistoryDocument = 'service_charge' | 'invoice' | 'receipt' | 'disbursement';
export type HistoryField =
  | 'label_id'
  | 'direction'
  | 'counterparty_id'
  | 'contract_id'
  | 'shipment_id'
  | 'transportation_id'
  | 'quantity_milli'
  | 'unit'
  | 'price_minor'
  | 'currency'
  | 'service_date'
  | 'invoice_currency'
  | 'invoice_rate.rate_millionths'
  | 'invoice_rate.date'
  | 'invoice_rate.source'
  | 'price_mode'
  | 'vat_mode'
  | 'vat_rate_basis_points'
  | 'net_minor'
  | 'vat_minor'
  | 'total_minor'
  | 'invoice_net_minor'
  | 'invoice_vat_minor'
  | 'invoice_total_minor'
  | 'note'
  | 'archived'
  | 'number'
  | 'invoice_date'
  | 'due_date'
  | 'own_company_id'
  | 'own_bank_account_id'
  | 'counterparty_bank_account_id'
  | 'kind'
  | 'payment_date'
  | 'amount_minor'
  | 'purpose'
  | 'charges'
  | 'invoice'
  | 'allocation';
export interface HistoryReference {
  source: 'service_charge' | 'invoice' | 'payment';
  id: string;
  name?: string;
}
export interface HistoryValue {
  value: string;
  format:
    'text' | 'date' | 'money' | 'quantity' | 'percent' | 'rate' | 'boolean' | 'enum' | 'reference';
  currency?: string;
  display_name?: string;
}
export interface HistoryCursor {
  source: 'service_charge' | 'invoice' | 'payment';
  entity_id: string;
  revision: number;
  occurred_at: string;
}
export interface HistoryChange {
  field: HistoryField;
  subject?: HistoryReference;
  before?: HistoryValue;
  after?: HistoryValue;
}
export interface HistoryEvent {
  cursor: HistoryCursor;
  actor: string;
  actor_name?: string;
  action: HistoryAction;
  document: HistoryDocument;
  reference?: HistoryReference;
  has_previous: boolean;
  changes: HistoryChange[];
}
export interface HistoryPage {
  items: HistoryEvent[];
  next_before?: HistoryCursor;
}
export function historyValue(
  value?: HistoryValue,
  locale: Locale = 'ru',
  field?: HistoryField,
  showReferenceId = false,
): string {
  if (!value) return '—';
  switch (value.format) {
    case 'boolean':
      return value.value === 'true'
        ? translate(locale, 'filter.yes')
        : value.value === 'false'
          ? translate(locale, 'filter.no')
          : value.value;
    case 'enum':
      return historyEnumValue(field, value.value, locale);
    case 'reference':
      return value.display_name
        ? value.display_name + (showReferenceId ? ` (${value.value})` : '')
        : value.value;
    case 'date':
      try {
        return CalendarDate.fromISO(value.value).toDisplayString();
      } catch {
        return value.value;
      }
    case 'money':
      return `${formatFixed(value.value, 2)}${value.currency ? ' ' + value.currency : ''}`;
    case 'quantity':
      return formatFixed(value.value, 3);
    case 'percent':
      return (
        formatFixed(value.value, 2)
          .replace(/,00$/, '')
          .replace(/(,\d)0$/, '$1') + '%'
      );
    case 'rate':
      return formatFixed(value.value, 6).replace(/,?0+$/, '');
    default:
      return value.value;
  }
}

const fieldKeys = {
  label_id: 'registryField.label_name',
  direction: 'registryField.direction',
  counterparty_id: 'registryField.counterparty_id',
  contract_id: 'registryField.contract_id',
  shipment_id: 'registryField.shipment_id',
  transportation_id: 'registryValue.unit.transportation',
  quantity_milli: 'history.field.quantity_milli',
  unit: 'registryField.unit',
  price_minor: 'history.field.price_minor',
  currency: 'finance.serviceCurrency',
  service_date: 'registryField.service_date',
  invoice_currency: 'finance.invoiceCurrency',
  'invoice_rate.rate_millionths': 'history.field.invoice_rate.rate_millionths',
  'invoice_rate.date': 'history.field.invoice_rate.date',
  'invoice_rate.source': 'history.field.invoice_rate.source',
  price_mode: 'finance.priceMode',
  vat_mode: 'registryField.vat_mode',
  vat_rate_basis_points: 'registryValue.vat_mode.rate',
  net_minor: 'finance.net',
  vat_minor: 'registryField.vat_mode',
  total_minor: 'registryField.total_minor',
  invoice_net_minor: 'history.field.invoice_net_minor',
  invoice_vat_minor: 'history.field.invoice_vat_minor',
  invoice_total_minor: 'history.field.invoice_total_minor',
  note: 'shipment.note',
  archived: 'filter.archived',
  number: 'registryField.number',
  invoice_date: 'registryField.invoice_date',
  due_date: 'registryField.due_date',
  own_company_id: 'forms.ownLegalEntityLabel',
  own_bank_account_id: 'history.field.own_bank_account_id',
  counterparty_bank_account_id: 'history.field.counterparty_bank_account_id',
  kind: 'history.field.kind',
  payment_date: 'registryField.payment_date',
  amount_minor: 'registryField.amount',
  purpose: 'forms.paymentPurpose',
  charges: 'history.field.charges',
  invoice: 'history.invoice',
  allocation: 'history.field.allocation',
} satisfies Record<HistoryField, MessageKey>;

const documentKeys = {
  service_charge: 'entity.serviceCharge',
  invoice: 'history.invoice',
  receipt: 'entity.receipt',
  disbursement: 'entity.disbursement',
} satisfies Record<HistoryDocument, MessageKey>;
const subjectKeys = {
  service_charge: 'entity.serviceCharge',
  invoice: 'history.invoice',
  payment: 'registryField.payment',
} satisfies Record<HistoryReference['source'], MessageKey>;
const actionKeys = {
  created: 'history.created',
  updated: 'history.updated',
  archived: 'history.archived',
  restored: 'history.restored',
} satisfies Partial<Record<HistoryAction, MessageKey>>;
const relatedActionKeys = {
  invoice_linked: 'history.invoiceLinked',
  invoice_unlinked: 'history.invoiceUnlinked',
  payment_allocated: 'history.paymentAllocated',
  payment_reallocated: 'history.paymentReallocated',
  payment_unallocated: 'history.paymentUnallocated',
} satisfies Partial<Record<HistoryAction, MessageKey>>;
export function historyTitle(event: HistoryEvent, locale: Locale): string {
  if (event.action in relatedActionKeys)
    return translate(locale, relatedActionKeys[event.action as keyof typeof relatedActionKeys], {
      number: event.reference?.name ?? event.reference?.id ?? '—',
    });
  const action = actionKeys[event.action as keyof typeof actionKeys];
  const document = documentKeys[event.document];
  return action && document
    ? translate(locale, 'history.documentTitle', {
        document: translate(locale, document),
        action: translate(locale, action),
      })
    : translate(locale, 'history.unknownEvent');
}
export function historyFieldLabel(
  change: HistoryChange,
  document: HistoryDocument,
  locale: Locale,
): string {
  let key: MessageKey = fieldKeys[change.field];
  // Shared snapshot keys have document-specific meanings.
  if (
    change.field === 'currency' &&
    document !== 'service_charge' &&
    change.subject?.source !== 'service_charge'
  )
    key = 'registryField.currency';
  if (change.field === 'number')
    key = document === 'invoice' ? 'registryField.invoiceNumber' : 'registryField.paymentNumber';
  const field = key ? translate(locale, key) : change.field;
  if (!change.subject) return field;
  return translate(locale, 'history.relatedField', {
    document: translate(locale, subjectKeys[change.subject.source]),
    name: change.subject.name ?? change.subject.id,
    field,
  });
}
function historyEnumValue(field: HistoryField | undefined, value: string, locale: Locale): string {
  if (field === 'direction' && (value === 'incoming' || value === 'outgoing'))
    return translate(locale, value === 'incoming' ? 'history.incoming' : 'history.outgoing');
  if (field === 'kind' && (value === 'receipt' || value === 'disbursement'))
    return translate(locale, value === 'receipt' ? 'entity.receipt' : 'entity.disbursement');
  if (field === 'price_mode' && (value === 'net' || value === 'gross'))
    return translate(locale, value === 'net' ? 'finance.net' : 'finance.gross');
  if (field === 'invoice_rate.source') {
    const keys: Record<string, MessageKey> = {
      identity: 'history.identityRate',
      cbr: 'finance.centralBank',
      manual: 'shipment.manual',
    };
    return keys[value] ? translate(locale, keys[value]) : value;
  }
  return field ? (registryValueLabel(field, value, locale) ?? value) : value;
}
export function historyChangeKey(change: HistoryChange): string {
  return `${change.subject?.source ?? ''}:${change.subject?.id ?? ''}:${change.field}`;
}
export function historyReferenceIdsDiffer(change: HistoryChange): boolean {
  return (
    change.before?.format === 'reference' &&
    change.after?.format === 'reference' &&
    change.before.value !== change.after.value &&
    change.before.display_name === change.after.display_name
  );
}
