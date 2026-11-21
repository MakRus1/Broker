import { quotationServiceKey } from './quotationComparison';
import type { ProposalLine } from '../proposals';
import type { Rfq, RfqQuote, RfqQuoteLine } from './types';
export function quotationLines(
  rfq: Rfq,
  quote: RfqQuote,
  lines: RfqQuoteLine[] = quote.lines,
): ProposalLine[] {
  return lines.map((line) => ({
    id: crypto.randomUUID(),
    label_id: line.label_id,
    unit: line.unit,
    quantity_milli: line.quantity_milli,
    supplier_id: quote.supplier_id,
    route_segment_id: rfq.input.route_segment_id,
    purchase: { ...line.price },
    sale: { ...line.price },
    quote_source: {
      rfq_id: rfq.input.id,
      revision: rfq.revision,
      quote_id: quote.id,
      line_id: line.id,
    },
    note: quote.note,
  }));
}

/** Replace sourced alternatives without overwriting manually entered services or client prices. */
export function replaceQuotedServices(
  existing: ProposalLine[],
  incoming: ProposalLine[],
): ProposalLine[] {
  const replacements = new Map(
    incoming.map((line) => [quotationServiceKey(line.route_segment_id, line.label_id), line]),
  );
  const consumed = new Set<string>();
  const result = existing.flatMap((line) => {
    const key = quotationServiceKey(line.route_segment_id, line.label_id);
    const replacement = line.quote_source ? replacements.get(key) : undefined;
    if (!replacement) return [line];
    if (consumed.has(key)) return [];
    consumed.add(key);
    return [
      {
        ...replacement,
        id: line.id,
        sale:
          line.unit === replacement.unit && line.quantity_milli === replacement.quantity_milli
            ? { ...line.sale }
            : replacement.sale,
      },
    ];
  });
  return [
    ...result,
    ...[...replacements.entries()].filter(([key]) => !consumed.has(key)).map(([, line]) => line),
  ];
}
