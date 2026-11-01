import { request } from '../../../shared/api/http';
import type { Session } from '../../../shared/api/session';
import type { ServiceCharge } from '../services/serviceCharges';
import type { InvoiceDetails, ServiceChargeInvoice } from './invoices';
import type { ServiceInvoiceSelection } from './ServiceInvoiceActions';

export type SelectableCharge = ServiceCharge & { invoice?: ServiceChargeInvoice };
export function bulkInvoiceProblem(items: SelectableCharge[]): string | undefined {
  if (!items.length) return 'Выберите услуги';
  if (items.length > 500) return 'В один счёт можно включить не больше 500 услуг';
  if (items.some((item) => item.archived || item.invoice))
    return 'Выберите действующие услуги, ещё не включённые в счёт';
  const first = items[0];
  if (
    items.some(
      (item) =>
        item.direction !== first.direction ||
        item.counterparty_id !== first.counterparty_id ||
        item.contract_id !== first.contract_id ||
        item.invoice_currency !== first.invoice_currency,
    )
  )
    return 'Для общего счёта выберите услуги одного направления, контрагента, договора и валюты счёта';
}

// Один save проверяет все услуги и редакцию счёта в одной серверной транзакции.
export async function attachBulkInvoice(
  ids: string[],
  selection: ServiceInvoiceSelection,
  session: Session,
): Promise<InvoiceDetails> {
  let body;
  if (selection.kind === 'new') {
    body = { ...selection.body, expected_revision: 0, service_charge_ids: ids };
  } else {
    const current = await request<InvoiceDetails>('/api/v1/invoices/get', session, {
      id: selection.invoice.id,
    });
    if (current.invoice.revision !== selection.invoice.revision)
      throw new Error('Счёт изменён. Выберите его заново');
    const invoice = current.invoice;
    body = {
      id: invoice.id,
      direction: invoice.direction,
      number: invoice.number,
      invoice_date: invoice.invoice_date,
      due_date: invoice.due_date,
      own_company_id: invoice.own_company_id,
      counterparty_id: invoice.counterparty_id,
      own_bank_account_id: invoice.own_bank_account_id,
      counterparty_bank_account_id: invoice.counterparty_bank_account_id,
      contract_id: invoice.contract_id,
      currency: invoice.currency,
      note: invoice.note,
      archived: invoice.archived,
      expected_revision: invoice.revision,
      service_charge_ids: [...new Set([...current.charges.map((item) => item.id), ...ids])],
    };
  }
  if (body.service_charge_ids.length > 500)
    throw new Error('В счёте может быть не больше 500 услуг');
  try {
    return await request<InvoiceDetails>('/api/v1/invoices/save', session, body);
  } catch (error) {
    // Потерянный ответ после commit не должен приводить к повторному созданию счёта.
    try {
      const saved = await request<InvoiceDetails>('/api/v1/invoices/get', session, { id: body.id });
      if (
        !saved.invoice.archived &&
        ids.every((id) => saved.charges.some((item) => item.id === id))
      )
        return saved;
    } catch {
      /* Сохраняем исходную ошибку. */
    }
    throw error;
  }
}
