import { proposalAmounts } from '../proposalAmounts';
import { quotationLineKey, quotationServiceKey } from './quotationComparison';
import type { Rfq, RfqQuote, RfqQuoteLine } from './types';

export type PurchaseGap = {
  segment: string;
  name: string;
  reason: 'expired' | 'volume' | 'currency' | 'missing';
};

/** Independent service prices only: no FX conversion, quantity inference or package discounts. */
export function minimumPurchase(items: Rfq[], calculationDate: string, segments: string[]) {
  const services = new Map<
    string,
    {
      segment: string;
      name: string;
      candidates: { rfq: Rfq; quote: RfqQuote; line: RfqQuoteLine }[];
    }
  >();
  for (const rfq of items) {
    if (rfq.input.status !== 'ready') continue;
    for (const quote of rfq.input.quotes)
      for (const line of quote.lines) {
        const key = quotationServiceKey(rfq.input.route_segment_id, line.label_id);
        const service = services.get(key) ?? {
          segment: rfq.input.route_segment_id,
          name: line.label_name ?? line.label_id,
          candidates: [],
        };
        service.candidates.push({ rfq, quote, line });
        services.set(key, service);
      }
  }
  const keys = new Set<string>();
  const gaps: PurchaseGap[] = [];
  for (const service of services.values()) {
    const available = service.candidates.filter(
      ({ quote }) => !quote.valid_until || quote.valid_until >= calculationDate,
    );
    const reason = !available.length
      ? 'expired'
      : new Set(available.map(({ line }) => JSON.stringify([line.unit, line.quantity_milli])))
            .size > 1
        ? 'volume'
        : new Set(available.map(({ line }) => line.price.currency)).size > 1
          ? 'currency'
          : undefined;
    if (reason) {
      gaps.push({ segment: service.segment, name: service.name, reason });
      continue;
    }
    const cheapest = available.reduce((best, candidate) =>
      proposalAmounts(candidate.line.quantity_milli, candidate.line.price).net <
      proposalAmounts(best.line.quantity_milli, best.line.price).net
        ? candidate
        : best,
    );
    keys.add(quotationLineKey(cheapest.rfq, cheapest.quote, cheapest.line));
  }
  for (const segment of segments)
    if (![...services.values()].some((service) => service.segment === segment))
      gaps.push({ segment, name: '', reason: 'missing' });
  return { keys, gaps };
}
