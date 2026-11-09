import type { ProposalLine } from '../proposals';
import type { Rfq, RfqQuote } from './types';
export function quotationLines(rfq: Rfq, quote: RfqQuote): ProposalLine[] {
  return quote.lines.map((line) => ({
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
