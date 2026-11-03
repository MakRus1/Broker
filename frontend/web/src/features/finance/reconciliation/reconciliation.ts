import { translate, type Locale } from '../../../../../shared/i18n/catalog';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { formatFixed } from '../services/serviceCharges';
export type ReconciliationKind =
  'outgoing_invoice' | 'incoming_invoice' | 'receipt' | 'disbursement';
export const reconciliationLabelKeys = {
  outgoing_invoice: 'forms.outgoingInvoice',
  incoming_invoice: 'forms.incomingInvoice',
  receipt: 'entity.receipt',
  disbursement: 'entity.disbursement',
} as const;
export type ReconciliationTotal = {
  currency: string;
  opening_minor: string;
  debit_minor: string;
  credit_minor: string;
  closing_minor: string;
};
export type ReconciliationOperation = {
  id: string;
  kind: ReconciliationKind;
  date: string;
  number: string;
  own_company_name: string;
  currency: string;
  debit_minor: string;
  credit_minor: string;
  balance_minor: string;
};
export type Reconciliation = {
  totals: ReconciliationTotal[];
  operations: ReconciliationOperation[];
};
export type ReconciliationContext = {
  counterparty: string;
  ownCompany: string;
  contract: string;
  from: string;
  to: string;
};
export function reconciliationLink(operation: ReconciliationOperation) {
  const base: Record<ReconciliationKind, string> = {
    outgoing_invoice: 'outgoing-invoices',
    incoming_invoice: 'incoming-invoices',
    receipt: 'receipts',
    disbursement: 'disbursements',
  };
  return `#/${base[operation.kind]}/${operation.id}`;
}
export function reconciliationCsv(
  data: Reconciliation,
  context: ReconciliationContext,
  locale: Locale = 'ru',
) {
  function cell(value: string) {
    const safe = /^[\s]*[=+\-@]/.test(value) ? `'${value}` : value;
    return `"${safe.replaceAll('"', '""')}"`;
  }
  const rows: string[][] = [
    [translate(locale, 'forms.reconciliationStatement')],
    [translate(locale, 'registryField.counterparty_id'), context.counterparty],
    [translate(locale, 'forms.ownLegalEntity'), context.ownCompany],
    [translate(locale, 'registryField.contract_id'), context.contract],
    [translate(locale, 'reconciliation.period'), context.from, context.to],
    [translate(locale, 'reconciliation.currentState')],
    [],
    [
      translate(locale, 'registryField.currency'),
      translate(locale, 'forms.date'),
      translate(locale, 'forms.document'),
      translate(locale, 'forms.ownLegalEntity'),
      translate(locale, 'forms.debit'),
      translate(locale, 'forms.credit'),
      translate(locale, 'forms.balance'),
    ],
  ];
  for (const total of data.totals)
    rows.push([
      total.currency,
      '',
      translate(locale, 'forms.openingBalance'),
      '',
      '',
      '',
      formatFixed(total.opening_minor, 2),
    ]);
  for (const item of data.operations)
    rows.push([
      item.currency,
      CalendarDate.fromISO(item.date).toDisplayString(),
      `${translate(locale, reconciliationLabelKeys[item.kind])} № ${item.number}`,
      item.own_company_name,
      formatFixed(item.debit_minor, 2),
      formatFixed(item.credit_minor, 2),
      formatFixed(item.balance_minor, 2),
    ]);
  for (const total of data.totals)
    rows.push([
      total.currency,
      '',
      translate(locale, 'reconciliation.turnover'),
      '',
      formatFixed(total.debit_minor, 2),
      formatFixed(total.credit_minor, 2),
      formatFixed(total.closing_minor, 2),
    ]);
  return '\ufeff' + rows.map((row) => row.map(cell).join(';')).join('\r\n');
}
