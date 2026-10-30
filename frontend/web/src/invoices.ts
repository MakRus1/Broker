import type { ServiceCharge } from './serviceCharges';
export type InvoiceDirection = 'incoming' | 'outgoing';
export interface Invoice {
  id: string;
  direction: InvoiceDirection;
  number: string;
  invoice_date: string;
  due_date: string | null;
  own_company_id: string;
  own_company_name: string;
  counterparty_id: string;
  counterparty_name: string;
  own_bank_account_id: string | null;
  counterparty_bank_account_id: string | null;
  contract_id: string | null;
  contract_number: string | null;
  currency: string;
  note: string | null;
  archived: boolean;
  revision: number;
  net_minor: number;
  vat_minor: number;
  total_minor: number;
}
export interface InvoiceDetails {
  invoice: Invoice;
  charges: ServiceCharge[];
}
export const invoiceRegistry = (direction: InvoiceDirection) => `/${direction}-invoices`;
export const invoiceTitle = (direction: InvoiceDirection) =>
  direction === 'incoming' ? 'Входящие счета' : 'Исходящие счета';

export interface ServiceChargeInvoice {
  service_charge_id: string;
  invoice_id: string;
  number: string;
  invoice_date: string;
  direction: InvoiceDirection;
}
