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

/** Quantities and units must match before supplier prices can be compared side by side. */
export function comparableQuotationServices(quotes: RfqQuote[]) {
  const services = new Map<
    string,
    {
      key: string;
      name: string;
      unit: RfqQuoteLine['unit'];
      quantity_milli: number;
      lines: Map<string, RfqQuoteLine[]>;
    }
  >();
  for (const quote of quotes) {
    for (const line of quote.lines) {
      const key = JSON.stringify([line.label_id, line.unit, line.quantity_milli]);
      let service = services.get(key);
      if (!service) {
        service = {
          key,
          name: line.label_name ?? line.label_id,
          unit: line.unit,
          quantity_milli: line.quantity_milli,
          lines: new Map(),
        };
        services.set(key, service);
      }
      service.lines.set(quote.id, [...(service.lines.get(quote.id) ?? []), line]);
    }
  }
  return [...services.values()];
}

export function quotationServiceKey(routeSegment: string | undefined, label: string): string {
  return JSON.stringify([routeSegment, label]);
}

export function selectQuotationAlternatives(
  previous: ReadonlySet<string>,
  items: Rfq[],
  keys: string[],
  checked: boolean,
): Set<string> {
  const services = new Map(
    items.flatMap((rfq) =>
      rfq.input.quotes.flatMap((quote) =>
        quote.lines.map(
          (line) =>
            [
              quotationLineKey(rfq, quote, line),
              quotationServiceKey(rfq.input.route_segment_id, line.label_id),
            ] as const,
        ),
      ),
    ),
  );
  const next = new Set(previous);
  for (const key of keys) {
    const service = services.get(key);
    if (!service) continue;
    if (checked) {
      for (const selected of next) {
        if (services.get(selected) === service) next.delete(selected);
      }
      next.add(key);
    } else next.delete(key);
  }
  return next;
}
