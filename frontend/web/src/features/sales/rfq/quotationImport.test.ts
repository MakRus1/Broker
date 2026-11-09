import { expect, it } from 'vitest';
import { quotationLines } from './quotationImport';
import type { Rfq } from './types';
it('keeps the quotation immutable while allowing independent sale pricing and combining legs', () => {
  const rfq: Rfq = {
    revision: 3,
    inquiry_number: 'ENQ-1',
    route_segment: { id: 'leg-a', type: 'sea' },
    input: {
      id: 'rfq-a',
      inquiry_id: 'inquiry',
      route_segment_id: 'leg-a',
      title: 'Sea',
      responsible_subject: 'owner',
      status: 'ready',
      quotes: [
        {
          id: 'option-a',
          title: 'Direct',
          supplier_id: 'supplier-a',
          lines: [
            {
              id: 'line-a',
              label_id: 'freight',
              unit: 'container',
              quantity_milli: 2000,
              price: { price_minor: 10000, currency: 'USD', price_mode: 'net', vat_mode: 'exempt' },
            },
          ],
        },
      ],
    },
  };
  const snapshot = structuredClone(rfq);
  const first = quotationLines(rfq, rfq.input.quotes[0])[0];
  const second = quotationLines(
    { ...rfq, input: { ...rfq.input, id: 'rfq-b', route_segment_id: 'leg-b' } },
    { ...rfq.input.quotes[0], supplier_id: 'supplier-b' },
  )[0];
  first.sale.price_minor = 15000;
  expect(first.purchase?.price_minor).toBe(10000);
  expect(rfq).toEqual(snapshot);
  expect(first.quote_source).toEqual({
    rfq_id: 'rfq-a',
    revision: 3,
    quote_id: 'option-a',
    line_id: 'line-a',
  });
  expect(first.id).not.toBe(second.id);
  expect([first, second].map((v) => [v.route_segment_id, v.supplier_id])).toEqual([
    ['leg-a', 'supplier-a'],
    ['leg-b', 'supplier-b'],
  ]);
});
