import type { FinancialLabel, VatMode } from '../labels/financialLabels';
export interface ServiceChargeScope {
  shipment_id: string | null;
  transportation_id: string | null;
}
export interface ServiceCharge extends ServiceChargeScope {
  id: string;
  label_id: string;
  label_name: string;
  direction: 'income' | 'expense';
  unit: FinancialLabel['unit'];
  counterparty_id: string;
  counterparty_name: string;
  contract_id: string | null;
  contract_number: string | null;
  quantity_milli: number;
  price_minor: number;
  price_mode: 'net' | 'gross';
  currency: string;
  service_date: string;
  invoice_currency: string;
  invoice_rate: ExchangeRate;
  invoice_net_minor: number;
  invoice_vat_minor: number;
  invoice_total_minor: number;
  vat_mode: VatMode;
  vat_rate_basis_points: number | null;
  net_minor: number;
  vat_minor: number;
  total_minor: number;
  note: string | null;
  archived: boolean;
  revision: number;
}
export type CurrencyTotal = { currency: string; income_minor: string; expense_minor: string };
export type ServiceChargeSummary = {
  own: CurrencyTotal[];
  transportations: CurrencyTotal[];
  combined: CurrencyTotal[];
};
export function parseFixed(text: string, digits: number, maximum: number): number | undefined {
  const match = new RegExp(`^(\\d+)(?:[.,](\\d{1,${digits}}))?$`).exec(text.trim());
  if (!match) return undefined;
  const value =
    BigInt(match[1]) * 10n ** BigInt(digits) + BigInt((match[2] ?? '').padEnd(digits, '0'));
  return value <= BigInt(maximum) ? Number(value) : undefined;
}
export function formatFixed(value: number | bigint | string, digits: number): string {
  const amount = BigInt(value);
  const sign = amount < 0n ? '−' : '';
  const absolute = amount < 0n ? -amount : amount;
  const scale = 10n ** BigInt(digits);
  return `${sign}${(absolute / scale).toLocaleString('ru-RU')},${String(absolute % scale).padStart(digits, '0')}`;
}
export function inputFixed(value: number, digits: number): string {
  return (
    (BigInt(value) / 10n ** BigInt(digits)).toString() +
    ',' +
    String(value % 10 ** digits).padStart(digits, '0')
  );
}

export type ExchangeRate = {
  rate_millionths: number;
  date: string;
  source: 'identity' | 'cbr' | 'manual';
};
export type ReportingSummary = {
  currency: string;
  summary: ServiceChargeSummary;
  missing_rates: { currency: string; date: string }[];
};
