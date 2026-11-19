import type { ProposalLine } from '../proposals';
import type { Rfq, RfqQuote, RfqQuoteLine } from './types';

export function quotationTotals(lines: RfqQuoteLine[]) {
  const totals = new Map<string, { currency: string; net: bigint; vat: bigint; total: bigint }>();
  for (const line of lines) {
    const amount = (BigInt(line.quantity_milli) * BigInt(line.price.price_minor) + 500n) / 1000n;
    const rate =
      line.price.vat_mode === 'exempt' ? 0n : BigInt(line.price.vat_rate_basis_points ?? 0);
    const divisor = line.price.price_mode === 'gross' ? 10000n + rate : 10000n;
    const vat = (amount * rate + divisor / 2n) / divisor;
    const net = line.price.price_mode === 'gross' ? amount - vat : amount;
    const previous = totals.get(line.price.currency) ?? {
      currency: line.price.currency,
      net: 0n,
      vat: 0n,
      total: 0n,
    };
    totals.set(line.price.currency, {
      currency: previous.currency,
      net: previous.net + net,
      vat: previous.vat + vat,
      total: previous.total + net + vat,
    });
  }
  return [...totals.values()].sort((a, b) => a.currency.localeCompare(b.currency));
}

export function quotationLineKey(rfq: Rfq, quote: RfqQuote, line: RfqQuoteLine): string {
  return `${rfq.input.id}:${quote.id}:${line.id}`;
}

export function importedQuotationKeys(lines: ProposalLine[]): Set<string> {
  return new Set(
    lines.flatMap(({ quote_source: source }) =>
      source ? [`${source.rfq_id}:${source.quote_id}:${source.line_id}`] : [],
    ),
  );
}
