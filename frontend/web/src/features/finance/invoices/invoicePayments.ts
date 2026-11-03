import { translate, type Locale } from '../../../../../shared/i18n/catalog';
import { request } from '../../../shared/api/http';
import type { InvoiceDirection } from './invoices';
import type { Session } from '../../../shared/api/session';
import type { Allocation, InvoiceBalance, PaymentDetails } from '../payments/payments';
import { parseFixed, inputFixed } from '../services/serviceCharges';
export function invoicePaymentLabels(
  locale: Locale = 'ru',
): Record<
  InvoiceDirection,
  { action: string; success: string; remainder: string; archived: string }
> {
  return {
    outgoing: {
      action: translate(locale, 'payment.allocateReceipt'),
      success: translate(locale, 'payment.receiptSaved'),
      remainder: translate(locale, 'payment.receiptRemainder'),
      archived: translate(locale, 'payment.receiptArchived'),
    },
    incoming: {
      action: translate(locale, 'payment.allocateDisbursement'),
      success: translate(locale, 'payment.disbursementSaved'),
      remainder: translate(locale, 'payment.disbursementRemainder'),
      archived: translate(locale, 'payment.disbursementArchived'),
    },
  };
}
export type PaymentTarget = Pick<InvoiceBalance, 'invoice' | 'outstanding_minor'>;
export function invoicePaymentProblem(
  targets: PaymentTarget[],
  locale: Locale = 'ru',
): string | undefined {
  if (!targets.length) return translate(locale, 'bulk.selectInvoices');
  if (targets.length > 100) return translate(locale, 'bulk.invoiceLimit');
  if (targets.some(({ invoice, outstanding_minor }) => invoice.archived || outstanding_minor <= 0))
    return translate(locale, 'bulk.openInvoices');
  const first = targets[0].invoice;
  if (
    targets.some(
      ({ invoice }) =>
        invoice.direction !== first.direction ||
        invoice.own_company_id !== first.own_company_id ||
        invoice.counterparty_id !== first.counterparty_id ||
        invoice.currency !== first.currency,
    )
  )
    return translate(locale, 'bulk.sameInvoiceParties');
}
export function automaticInvoiceAmounts(
  targets: PaymentTarget[],
  available: number,
): Record<string, string> {
  let remaining = available;
  return Object.fromEntries(
    targets.map(({ invoice, outstanding_minor }) => {
      const amount = Math.min(remaining, outstanding_minor);
      remaining -= amount;
      return [invoice.id, inputFixed(amount, 2)];
    }),
  );
}
export function invoicePaymentAdditions(
  targets: PaymentTarget[],
  amounts: Record<string, string>,
  available: number,
  locale: Locale = 'ru',
): Allocation[] {
  const problem = invoicePaymentProblem(targets, locale);
  if (problem) throw new Error(problem);
  const result: Allocation[] = [];
  let remaining = available;
  for (const target of targets) {
    const amount = parseFixed(amounts[target.invoice.id] ?? '', 2, target.outstanding_minor);
    if (amount === undefined)
      throw new Error(translate(locale, 'payment.amountRange', { number: target.invoice.number }));
    if (amount > remaining) throw new Error(translate(locale, 'payment.overAllocated'));
    remaining -= amount;
    if (amount > 0) result.push({ invoice_id: target.invoice.id, amount_minor: amount });
  }
  if (!result.length) throw new Error(translate(locale, 'payment.positiveAmount'));
  return result;
}
export async function saveInvoicePayments(
  details: PaymentDetails,
  additions: Allocation[],
  session: Session,
  locale: Locale = 'ru',
) {
  const allocations = details.allocations.map((item) => ({ ...item }));
  for (const addition of additions) {
    const old = allocations.find((item) => item.invoice_id === addition.invoice_id);
    if (old) old.amount_minor += addition.amount_minor;
    else allocations.push(addition);
  }
  if (allocations.length > 100) throw new Error(translate(locale, 'payment.invoiceLimit'));
  const p = details.payment;
  // Редакция взята при выборе платежа: параллельные изменения не перезаписываем.
  return request<PaymentDetails>('/api/v1/payments/save', session, {
    id: p.id,
    kind: p.kind,
    number: p.number,
    payment_date: p.payment_date,
    own_company_id: p.own_company_id,
    counterparty_id: p.counterparty_id,
    own_bank_account_id: p.own_bank_account_id,
    counterparty_bank_account_id: p.counterparty_bank_account_id,
    currency: p.currency,
    amount_minor: p.amount_minor,
    purpose: p.purpose,
    note: p.note,
    archived: p.archived,
    expected_revision: p.revision,
    allocations,
  });
}
