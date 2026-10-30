import type { Invoice } from './invoices';
export type PaymentKind = 'receipt' | 'disbursement';
export type Payment = {
  id: string;
  kind: PaymentKind;
  number: string;
  payment_date: string;
  own_company_id: string;
  own_company_name: string;
  counterparty_id: string;
  counterparty_name: string;
  own_bank_account_id: string | null;
  counterparty_bank_account_id: string | null;
  currency: string;
  amount_minor: number;
  purpose: string | null;
  note: string | null;
  archived: boolean;
  revision: number;
};
export type Allocation = { invoice_id: string; amount_minor: number };
export type PaymentDetails = {
  payment: Payment;
  allocations: Allocation[];
  allocated_minor: number;
  unallocated_minor: number;
};
export type InvoiceBalance = {
  payments: { payment: Payment; amount_minor: number }[];
  invoice: Invoice;
  paid_minor: number;
  outstanding_minor: number;
};
export const paymentBase = (kind: PaymentKind) =>
  kind === 'receipt' ? '/receipts' : '/disbursements';
