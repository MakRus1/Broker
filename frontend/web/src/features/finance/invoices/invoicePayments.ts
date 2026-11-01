import { request } from '../../../shared/api/http';
import type { InvoiceDirection } from './invoices';
import type { Session } from '../../../shared/api/session';
import type { Allocation, InvoiceBalance, PaymentDetails } from '../payments/payments';
import { parseFixed, inputFixed } from '../services/serviceCharges';
export const invoicePaymentLabels: Record<
  InvoiceDirection,
  { action: string; success: string; remainder: string; archived: string }
> = {
  outgoing: {
    action: 'Распределить поступление',
    success: 'Распределение поступления сохранено',
    remainder: 'Останется в поступлении',
    archived: 'Платёж отправлен в архив. Выберите другое поступление',
  },
  incoming: {
    action: 'Распределить списание',
    success: 'Распределение списания сохранено',
    remainder: 'Останется в списании',
    archived: 'Платёж отправлен в архив. Выберите другое списание',
  },
};
export type PaymentTarget = Pick<InvoiceBalance, 'invoice' | 'outstanding_minor'>;
export function invoicePaymentProblem(targets: PaymentTarget[]): string | undefined {
  if (!targets.length) return 'Выберите счета';
  if (targets.length > 100) return 'За один раз можно оплатить не больше 100 счетов';
  if (targets.some(({ invoice, outstanding_minor }) => invoice.archived || outstanding_minor <= 0))
    return 'Выберите действующие счета с остатком к оплате';
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
    return 'Выберите счета с одним своим юрлицом, контрагентом, валютой и направлением';
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
): Allocation[] {
  const problem = invoicePaymentProblem(targets);
  if (problem) throw new Error(problem);
  const result: Allocation[] = [];
  let remaining = available;
  for (const target of targets) {
    const amount = parseFixed(amounts[target.invoice.id] ?? '', 2, target.outstanding_minor);
    if (amount === undefined)
      throw new Error(`Счёт № ${target.invoice.number}: укажите сумму от нуля до остатка счёта`);
    if (amount > remaining) throw new Error('Распределение превышает свободный остаток платежа');
    remaining -= amount;
    if (amount > 0) result.push({ invoice_id: target.invoice.id, amount_minor: amount });
  }
  if (!result.length) throw new Error('Укажите положительную сумму хотя бы для одного счёта');
  return result;
}
export async function saveInvoicePayments(
  details: PaymentDetails,
  additions: Allocation[],
  session: Session,
) {
  const allocations = details.allocations.map((item) => ({ ...item }));
  for (const addition of additions) {
    const old = allocations.find((item) => item.invoice_id === addition.invoice_id);
    if (old) old.amount_minor += addition.amount_minor;
    else allocations.push(addition);
  }
  if (allocations.length > 100)
    throw new Error('В платеже может быть не больше 100 оплачиваемых счетов');
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
