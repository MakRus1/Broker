import { expect, it } from 'vitest';
import {
  comparableQuotationServices,
  importedQuotationKeys,
  quotationLineKey,
  quotationTotals,
} from './quotationComparison';
import { quotationLines } from './quotationImport';
import type { Rfq, RfqQuoteLine } from './types';
const line: RfqQuoteLine = {
  id: 'freight',
  label_id: 'freight',
  label_name: 'Фрахт',
  unit: 'container',
  quantity_milli: 1500,
  price: {
    price_minor: 101,
    currency: 'RUB',
    price_mode: 'net',
    vat_mode: 'rate',
    vat_rate_basis_points: 2200,
  },
};
it('rounds each line before adding VAT and keeps currencies separate', () => {
  const gross: RfqQuoteLine = {
    ...line,
    id: 'gross',
    quantity_milli: 1000,
    price: { ...line.price, price_mode: 'gross', price_minor: 12200 },
  };
  const exempt: RfqQuoteLine = {
    ...line,
    id: 'usd',
    quantity_milli: 2000,
    price: { price_minor: 500, currency: 'USD', price_mode: 'net', vat_mode: 'exempt' },
  };
  expect(quotationTotals([line, gross, exempt])).toEqual([
    { currency: 'RUB', net: 10152n, vat: 2233n, total: 12385n },
    { currency: 'USD', net: 1000n, vat: 0n, total: 1000n },
  ]);
  expect(
    quotationTotals([
      { ...line, price: { ...line.price, price_minor: 100000000 }, quantity_milli: 10000000 },
    ])[0].total,
  ).toBe(1220000000000n);
});
it('imports only selected services and recognizes their source across RFQ revisions', () => {
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
      quotes: [],
    },
  };
  const quote = {
    id: 'quote-a',
    title: 'Direct',
    supplier_id: 'supplier',
    lines: [line, { ...line, id: 'handling' }],
  };
  const imported = quotationLines(rfq, quote, [quote.lines[1]]);
  expect(imported).toHaveLength(1);
  expect(imported[0].quote_source?.line_id).toBe('handling');
  const added = importedQuotationKeys(imported);
  expect(added.has(quotationLineKey({ ...rfq, revision: 4 }, quote, quote.lines[1]))).toBe(true);
  expect(added.has(quotationLineKey(rfq, quote, line))).toBe(false);
  expect(added.has(quotationLineKey(rfq, { ...quote, id: 'quote-b' }, quote.lines[1]))).toBe(false);
  expect(importedQuotationKeys([{ ...imported[0], quote_source: undefined }]).size).toBe(0);
});

it('compares only matching label, unit and quantity, retaining missing and repeated supplier lines', () => {
  const quotes = [
    {
      id: 'a',
      title: 'A',
      supplier_id: 'a',
      lines: [line, { ...line, id: 'repeat' }, { ...line, id: 'quantity', quantity_milli: 1000 }],
    },
    {
      id: 'b',
      title: 'B',
      supplier_id: 'b',
      lines: [
        { ...line, id: 'other', price: { ...line.price, currency: 'USD' } },
        { ...line, id: 'unit', unit: 'transportation' as const },
        { ...line, id: 'label', label_id: 'handling' },
      ],
    },
  ];
  const services = comparableQuotationServices(quotes);
  expect(services).toHaveLength(4);
  expect(services[0].lines.get('a')?.map((value) => value.id)).toEqual(['freight', 'repeat']);
  expect(services[0].lines.get('b')?.[0].price.currency).toBe('USD');
  expect(services[1].lines.has('b')).toBe(false);
  expect(services[2].unit).toBe('transportation');
  expect(quotes[0].lines).toHaveLength(3);
});
