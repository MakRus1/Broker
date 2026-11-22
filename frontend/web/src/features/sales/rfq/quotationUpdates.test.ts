import { expect, it } from 'vitest';
import { quotationUpdates } from './quotationUpdates';
import { quotationLines } from './quotationImport';
import type { Rfq } from './types';
const rfq: Rfq = {
  revision: 1,
  inquiry_number: 'ENQ',
  route_segment: { id: 'sea', type: 'sea' },
  input: {
    id: 'rfq',
    inquiry_id: 'inquiry',
    route_segment_id: 'sea',
    title: 'Sea',
    responsible_subject: 'author',
    status: 'ready',
    quotes: [
      {
        id: 'quote',
        title: 'Direct',
        supplier_id: 'supplier',
        valid_until: '2026-11-30',
        lines: [
          {
            id: 'line',
            label_id: 'freight',
            unit: 'container',
            quantity_milli: 1000,
            price: { price_minor: 10000, currency: 'RUB', price_mode: 'net', vat_mode: 'exempt' },
          },
        ],
      },
    ],
  },
};
it('detects a newer revision without changing the proposal snapshot', () => {
  const lines = quotationLines(rfq, rfq.input.quotes[0]);
  expect(quotationUpdates(lines, [rfq], '2026-11-22')).toEqual([]);
  const current = structuredClone(rfq);
  current.revision = 2;
  current.input.quotes[0].lines[0].price.price_minor = 12000;
  const updates = quotationUpdates(lines, [current], '2026-11-22');
  expect(updates).toHaveLength(1);
  expect(updates[0].replacement?.purchase?.price_minor).toBe(12000);
  expect(lines[0].purchase?.price_minor).toBe(10000);
});
it('keeps removed, expired and unready sources visible but unavailable', () => {
  const lines = quotationLines(rfq, rfq.input.quotes[0]);
  for (const mode of ['removed', 'expired', 'unready']) {
    const current = structuredClone(rfq);
    current.revision = 2;
    if (mode === 'removed') current.input.quotes[0].lines = [];
    if (mode === 'expired') current.input.quotes[0].valid_until = '2026-11-21';
    if (mode === 'unready') current.input.status = 'in_progress';
    const updates = quotationUpdates(lines, [current], '2026-11-22');
    expect(updates).toHaveLength(1);
    expect(updates[0].replacement).toBeUndefined();
  }
  expect(
    quotationUpdates(
      [{ ...lines[0], quote_source: undefined }],
      [{ ...rfq, revision: 2 }],
      '2026-11-22',
    ),
  ).toEqual([]);
});
