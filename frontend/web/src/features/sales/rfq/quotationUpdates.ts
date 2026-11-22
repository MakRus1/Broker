import type { ShipmentRouteSegment } from '../../routes/types';
import { quotationRouteState } from './quotationRoute';
import type { ProposalLine } from '../proposals';
import { quotationLines } from './quotationImport';
import type { Rfq } from './types';

export function quotationUpdates(
  lines: ProposalLine[],
  items: Rfq[],
  date: string,
  segments?: ShipmentRouteSegment[],
) {
  return lines.flatMap((line) => {
    const source = line.quote_source;
    if (!source) return [];
    const rfq = items.find((item) => item.input.id === source.rfq_id);
    if (!rfq || rfq.revision <= source.revision) return [];
    const quote = rfq.input.quotes.find((item) => item.id === source.quote_id);
    const current = quote?.lines.find((item) => item.id === source.line_id);
    const available =
      rfq.input.status === 'ready' &&
      (!segments || quotationRouteState(rfq, segments) === 'current') &&
      quote &&
      current &&
      (!quote.valid_until || quote.valid_until >= date);
    return [
      {
        line,
        rfq,
        quote,
        replacement: available ? quotationLines(rfq, quote, [current])[0] : undefined,
      },
    ];
  });
}
