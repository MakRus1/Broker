import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { formatFixed } from '../services/serviceCharges';
export type ReconciliationKind =
  'outgoing_invoice' | 'incoming_invoice' | 'receipt' | 'disbursement';
export const reconciliationLabels: Record<ReconciliationKind, string> = {
  outgoing_invoice: 'Исходящий счёт',
  incoming_invoice: 'Входящий счёт',
  receipt: 'Поступление',
  disbursement: 'Списание',
};
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
export function reconciliationCsv(data: Reconciliation, context: ReconciliationContext) {
  function cell(value: string) {
    const safe = /^[\s]*[=+\-@]/.test(value) ? `'${value}` : value;
    return `"${safe.replaceAll('"', '""')}"`;
  }
  const rows: string[][] = [
    ['Акт сверки'],
    ['Контрагент', context.counterparty],
    ['Наше юрлицо', context.ownCompany],
    ['Договор', context.contract],
    ['Период', context.from, context.to],
    ['Сформировано по текущему состоянию документов'],
    [],
    ['Валюта', 'Дата', 'Документ', 'Наше юрлицо', 'Дебет', 'Кредит', 'Сальдо'],
  ];
  for (const total of data.totals)
    rows.push([
      total.currency,
      '',
      'Начальное сальдо',
      '',
      '',
      '',
      formatFixed(total.opening_minor, 2),
    ]);
  for (const item of data.operations)
    rows.push([
      item.currency,
      CalendarDate.fromISO(item.date).toDisplayString(),
      `${reconciliationLabels[item.kind]} № ${item.number}`,
      item.own_company_name,
      formatFixed(item.debit_minor, 2),
      formatFixed(item.credit_minor, 2),
      formatFixed(item.balance_minor, 2),
    ]);
  for (const total of data.totals)
    rows.push([
      total.currency,
      '',
      'Обороты / конечное сальдо',
      '',
      formatFixed(total.debit_minor, 2),
      formatFixed(total.credit_minor, 2),
      formatFixed(total.closing_minor, 2),
    ]);
  return '\ufeff' + rows.map((row) => row.map(cell).join(';')).join('\r\n');
}
