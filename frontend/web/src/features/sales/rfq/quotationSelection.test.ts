import { expect, it } from 'vitest';
import { quotationLines, replaceQuotedServices } from './quotationImport';
import { quotationLineKey, selectQuotationAlternatives } from './quotationComparison';
import type { Rfq } from './types';

function request(id: string, segment: string): Rfq {
  const price = {
    price_minor: 10000,
    currency: 'USD',
    price_mode: 'net' as const,
    vat_mode: 'exempt' as const,
  };
  return {
    revision: 2,
    inquiry_number: 'ENQ',
    route_segment: { id: segment, type: 'sea' },
    input: {
      id,
      inquiry_id: 'inquiry',
      route_segment_id: segment,
      title: id,
      status: 'ready',
      responsible_subject: 'author',
      quotes: ['a', 'b'].map((supplier, index) => ({
        id: supplier,
        title: supplier,
        supplier_id: supplier,
        lines: ['freight', 'handling'].map((label) => ({
          id: label,
          label_id: label,
          unit: 'container',
          quantity_milli: 1000,
          price: { ...price, price_minor: 10000 + index * 5000 },
        })),
      })),
    },
  };
}
const key = (rfq: Rfq, quote = 0, line = 0) =>
  quotationLineKey(rfq, rfq.input.quotes[quote], rfq.input.quotes[quote].lines[line]);

it('replaces competing selections across RFQs for one leg, retaining other services and legs', () => {
  const a = request('one', 'sea');
  const b = request('two', 'sea');
  const other = request('other', 'road');
  const initial = new Set([key(a), key(a, 0, 1), key(other)]);
  const selected = selectQuotationAlternatives(initial, [a, b, other], [key(b, 1)], true);
  expect([...selected]).toEqual([key(a, 0, 1), key(other), key(b, 1)]);
  expect(initial.has(key(a))).toBe(true);
  expect(
    selectQuotationAlternatives(selected, [a, b, other], [key(b, 1)], false).has(key(b, 1)),
  ).toBe(false);
});
it('bulk selection replaces all competing services, without mixing supplier alternatives', () => {
  const rfq = request('one', 'sea');
  const selected = selectQuotationAlternatives(
    new Set([key(rfq), key(rfq, 0, 1)]),
    [rfq],
    [key(rfq, 1), key(rfq, 1, 1)],
    true,
  );
  expect([...selected]).toEqual([key(rfq, 1), key(rfq, 1, 1)]);
});
it('replaces imported purchase and source, retaining client pricing and manual services', () => {
  const rfq = request('one', 'sea');
  const existing = quotationLines(rfq, rfq.input.quotes[0]);
  existing[0].sale = { ...existing[0].sale, price_minor: 25000, currency: 'RUB' };
  const manual = { ...existing[0], id: 'manual', quote_source: undefined };
  const incoming = quotationLines(rfq, rfq.input.quotes[1], [rfq.input.quotes[1].lines[0]]);
  const result = replaceQuotedServices([...existing, manual], incoming);
  expect(result).toHaveLength(3);
  expect(result[0]).toMatchObject({
    id: existing[0].id,
    supplier_id: 'b',
    purchase: { price_minor: 15000 },
    sale: { price_minor: 25000, currency: 'RUB' },
    quote_source: { quote_id: 'b', revision: 2 },
  });
  expect(result[1]).toEqual(existing[1]);
  expect(result[2]).toEqual(manual);
  expect(existing[0].supplier_id).toBe('a');
  incoming[0].quantity_milli = 2000;
  expect(replaceQuotedServices(existing, incoming)[0].sale.price_minor).toBe(15000);
});
it('repairs an already duplicated imported service when replacing it, and appends new legs', () => {
  const rfq = request('one', 'sea');
  const old = quotationLines(rfq, rfq.input.quotes[0], [rfq.input.quotes[0].lines[0]]);
  const replacement = quotationLines(rfq, rfq.input.quotes[1], [rfq.input.quotes[1].lines[0]]);
  const other = request('other', 'road');
  const road = quotationLines(other, other.input.quotes[0], [other.input.quotes[0].lines[0]]);
  const result = replaceQuotedServices(
    [...old, { ...old[0], id: 'duplicate' }],
    [...replacement, ...road],
  );
  expect(result).toHaveLength(2);
  expect(result.map((line) => line.route_segment_id)).toEqual(['sea', 'road']);
});

it('applies an explicitly selected newer quotation revision while retaining client price', () => {
  const old = request('one', 'sea');
  const existing = quotationLines(old, old.input.quotes[0]);
  existing[0].sale.price_minor = 25000;
  const current = structuredClone(old);
  current.revision += 1;
  current.input.quotes[0].lines[0].price.price_minor = 18000;
  const incoming = quotationLines(current, current.input.quotes[0], [
    current.input.quotes[0].lines[0],
  ]);
  const updated = replaceQuotedServices(existing, incoming);
  expect(updated[0]).toMatchObject({
    id: existing[0].id,
    purchase: { price_minor: 18000 },
    sale: { price_minor: 25000 },
    quote_source: { revision: current.revision },
  });
  expect(existing[0].quote_source?.revision).toBe(old.revision);
});
