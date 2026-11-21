import { expect, it } from 'vitest';
import { minimumPurchase } from './minimumPurchase';
import type { Rfq, RfqQuoteLine } from './types';
const line: RfqQuoteLine = {
  id: 'line',
  label_id: 'freight',
  unit: 'container',
  quantity_milli: 1000,
  price: { price_minor: 100, currency: 'RUB', price_mode: 'net', vat_mode: 'exempt' },
};
function rfq(id: string, lines: RfqQuoteLine[], valid_until?: string): Rfq {
  return {
    revision: 1,
    inquiry_number: 'ENQ',
    route_segment: { id: 'sea', type: 'sea' },
    input: {
      id,
      inquiry_id: 'inquiry',
      route_segment_id: 'sea',
      title: id,
      responsible_subject: 'author',
      status: 'ready',
      quotes: [{ id: 'quote', supplier_id: id, title: id, valid_until, lines }],
    },
  };
}
it('compares net costs after VAT and never picks an expired lower price', () => {
  const result = minimumPurchase(
    [
      rfq('net', [line]),
      rfq('gross', [
        {
          ...line,
          price: {
            ...line.price,
            price_minor: 110,
            price_mode: 'gross',
            vat_mode: 'rate',
            vat_rate_basis_points: 2200,
          },
        },
      ]),
      rfq('expired', [{ ...line, price: { ...line.price, price_minor: 1 } }], '2026-11-20'),
    ],
    '2026-11-21',
    ['sea'],
  );
  expect([...result.keys]).toEqual(['gross:quote:line']);
  expect(result.gaps).toEqual([]);
});
it('does not guess exchange rates or compare different volumes', () => {
  const result = minimumPurchase(
    [rfq('a', [line]), rfq('b', [{ ...line, price: { ...line.price, currency: 'USD' } }])],
    '2026-11-21',
    ['sea'],
  );
  expect(result.keys.size).toBe(0);
  expect(result.gaps[0].reason).toBe('currency');
  expect(
    minimumPurchase(
      [rfq('a', [line]), rfq('b', [{ ...line, quantity_milli: 2000 }])],
      '2026-11-21',
      ['sea'],
    ).gaps[0].reason,
  ).toBe('volume');
});
it('reports expired services and route segments with no ready quote', () => {
  const result = minimumPurchase(
    [
      rfq('old', [line], '2026-11-20'),
      { ...rfq('draft', [line]), input: { ...rfq('draft', [line]).input, status: 'in_progress' } },
    ],
    '2026-11-21',
    ['sea', 'road'],
  );
  expect(result.keys.size).toBe(0);
  expect(result.gaps.map((gap) => gap.reason)).toEqual(['expired', 'missing']);
});
it('chooses services independently across suppliers with stable ties and accepts expiry on calculation day', () => {
  const handling = {
    ...line,
    id: 'handling',
    label_id: 'handling',
    price: { ...line.price, price_minor: 200 },
  };
  const result = minimumPurchase(
    [
      rfq('a', [line, handling], '2026-11-21'),
      rfq('b', [line, { ...handling, price: { ...handling.price, price_minor: 150 } }]),
    ],
    '2026-11-21',
    ['sea'],
  );
  expect([...result.keys]).toEqual(['a:quote:line', 'b:quote:handling']);
});
