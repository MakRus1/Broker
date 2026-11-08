import type { FinancialLabel } from '../finance/labels/financialLabels';
export type ProposalPrice = {
  price_minor: number;
  currency: string;
  price_mode: 'net' | 'gross';
  vat_mode: 'exempt' | 'rate';
  vat_rate_basis_points?: number;
};
export type ProposalLine = {
  id: string;
  label_id: string;
  unit: FinancialLabel['unit'];
  quantity_milli: number;
  supplier_id?: string;
  route_segment_id?: string;
  sale: ProposalPrice;
  purchase?: ProposalPrice;
  quote_source?: { rfq_id: string; revision: number; quote_id: string; line_id: string };
  note?: string;
};
export type ProposalInput = {
  id: string;
  inquiry_id: string;
  title: string;
  calculation_date: string;
  valid_until?: string;
  delivery_days?: number;
  note?: string;
  lines: ProposalLine[];
};
export type ProposalAmounts = { net_minor: number; vat_minor: number; total_minor: number };
export type ProposalTotal = { currency: string; sale: ProposalAmounts; purchase: ProposalAmounts };
export type Proposal = {
  input: ProposalInput;
  customer_name: string;
  inquiry_revision: number;
  revision: number;
  status: 'draft' | 'offered' | 'accepted';
  reporting_currency: string;
  rates: { currency: string; rate_millionths: number }[];
  lines: {
    input: ProposalLine;
    label_name: string;
    supplier_name?: string;
    sale: ProposalAmounts;
    purchase?: ProposalAmounts;
  }[];
  totals: ProposalTotal[];
  reporting: ProposalTotal;
};
export function proposalMargin(total: ProposalTotal): string {
  if (!total.sale.net_minor) return '—';
  return (
    (((total.sale.net_minor - total.purchase.net_minor) / total.sale.net_minor) * 100).toFixed(1) +
    '%'
  );
}
export function copyProposal(value: Proposal): ProposalInput {
  return { ...structuredClone(value.input), id: crypto.randomUUID() };
}
