import { proposalAmounts } from '../proposalAmounts';
import type { ProposalLine } from '../proposals';
import type { Rfq, RfqQuote, RfqQuoteLine } from './types';

export function quotationTotals(lines: RfqQuoteLine[]) {
  const totals = new Map<string, { currency: string; net: bigint; vat: bigint; total: bigint }>();
  for (const line of lines) {
    const { net, vat } = proposalAmounts(line.quantity_milli, line.price);
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
