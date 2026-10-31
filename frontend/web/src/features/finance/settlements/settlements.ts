import type { ColumnFilter, RegistryScope } from '../../registry/registryFilters';

export type SettlementFilters = {
  own_company_id: string;
  contract_id: string;
  currency: string;
};
export type SettlementTotal = {
  currency: string;
  receivable_minor: string;
  payable_minor: string;
  overdue_receivable_minor: string;
  overdue_payable_minor: string;
  unallocated_receipts_minor: string;
  unallocated_disbursements_minor: string;
};
export type SettlementView = 'outgoing' | 'incoming' | 'receipt' | 'disbursement';
export function settlementScope(
  counterparty: string,
  filters: SettlementFilters,
  view: SettlementView,
  overdue: boolean,
): RegistryScope {
  const invoices = view === 'outgoing' || view === 'incoming';
  const fields: ColumnFilter[] = [
    { field: 'counterparty_id', values: [counterparty] },
    { field: 'archived', values: ['false'] },
  ];
  if (filters.own_company_id)
    fields.push({ field: 'own_company_id', values: [filters.own_company_id] });
  if (filters.currency) fields.push({ field: 'currency', values: [filters.currency] });
  if (invoices) {
    fields.push({ field: 'payment_status', values: ['unpaid', 'partial'] });
    if (filters.contract_id) fields.push({ field: 'contract_id', values: [filters.contract_id] });
    if (overdue) fields.push({ field: 'overdue', values: ['true'] });
  } else {
    fields.push({ field: 'has_unallocated', values: ['true'] });
  }
  return { key: `settlements:${counterparty}`, filters: fields };
}
