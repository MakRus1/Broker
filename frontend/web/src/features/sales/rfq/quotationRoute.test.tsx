import { renderToStaticMarkup } from 'react-dom/server';
import { expect, it } from 'vitest';
import { quotationRouteState } from './quotationRoute';
import { RfqServiceComparison } from './RfqServiceComparison';
import { quotationUpdates } from './quotationUpdates';
import { quotationLines } from './quotationImport';
import type { Rfq } from './types';
const rfq: Rfq = {
  revision: 1,
  inquiry_number: 'ENQ',
  route_segment: {
    id: 'road',
    type: 'international_road',
    plan: { origin_name: 'A', destination_name: 'B' },
  },
  input: {
    id: 'rfq',
    inquiry_id: 'inquiry',
    route_segment_id: 'road',
    title: 'Road',
    responsible_subject: 'author',
    status: 'ready',
    quotes: [
      {
        id: 'quote',
        title: 'Direct',
        supplier_id: 'supplier',
        lines: [
          {
            id: 'line',
            label_id: 'label',
            unit: 'transportation',
            quantity_milli: 1000,
            price: { currency: 'RUB', price_minor: 10000, price_mode: 'net', vat_mode: 'exempt' },
          },
        ],
      },
    ],
  },
};
it('distinguishes removed segments and changes to route details, including dates', () => {
  expect(quotationRouteState(rfq, [])).toBe('removed');
  expect(quotationRouteState(rfq, [rfq.route_segment])).toBe('current');
  expect(
    quotationRouteState(rfq, [
      { ...rfq.route_segment, plan: { ...rfq.route_segment.plan, arrival_date: '2026-12-01' } },
    ]),
  ).toBe('changed');
});
it('ignores object key order and omitted optional fields', () => {
  expect(
    quotationRouteState(rfq, [
      {
        type: 'international_road',
        plan: { destination_name: 'B', origin_name: 'A', arrival_date: undefined },
        id: 'road',
      },
    ]),
  ).toBe('current');
});
it('disables both individual choices and selecting the whole outdated quotation', () => {
  const html = renderToStaticMarkup(
    <RfqServiceComparison
      value={rfq}
      selection={{
        routeUnavailable: true,
        selected: new Set(),
        added: new Set(),
        calculationDate: '2026-11-22',
        onToggle: () => {},
      }}
    />,
  );
  const inputs = html.match(/<input[^>]*>/g)!;
  expect(inputs).toHaveLength(2);
  expect(inputs.every((input) => input.includes('disabled'))).toBe(true);
});
it('does not offer a newer source revision if the route changed', () => {
  const lines = quotationLines(rfq, rfq.input.quotes[0]);
  const newer = { ...rfq, revision: 2 };
  expect(
    quotationUpdates(lines, [newer], '2026-11-22', [rfq.route_segment])[0].replacement,
  ).toBeDefined();
  expect(quotationUpdates(lines, [newer], '2026-11-22', [])[0].replacement).toBeUndefined();
});

it('detects changes to sea ETA without changing ports', () => {
  const sea: Rfq = {
    ...rfq,
    route_segment: { id: 'road', type: 'sea', sea: { eta: '2026-12-20' } },
  };
  expect(quotationRouteState(sea, [{ ...sea.route_segment, sea: { eta: '2026-12-25' } }])).toBe(
    'changed',
  );
});
