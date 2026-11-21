import { renderToStaticMarkup } from 'react-dom/server';
import { expect, it } from 'vitest';
import { RfqServiceComparison } from './RfqServiceComparison';
import type { Rfq } from './types';

it('disables expired and imported services and distinguishes missing services', () => {
  const line = {
    id: 'line',
    label_id: 'freight',
    label_name: 'Фрахт',
    unit: 'container' as const,
    quantity_milli: 2000,
    price: {
      price_minor: 10000,
      currency: 'USD',
      price_mode: 'net' as const,
      vat_mode: 'exempt' as const,
    },
  };
  const rfq: Rfq = {
    revision: 1,
    inquiry_number: 'ENQ-1',
    route_segment: { id: 'leg', type: 'sea' },
    input: {
      id: 'rfq',
      inquiry_id: 'inquiry',
      route_segment_id: 'leg',
      title: 'Sea',
      responsible_subject: 'author',
      status: 'ready',
      quotes: [
        {
          id: 'expired',
          supplier_id: 'a',
          supplier_name: 'A',
          title: 'Old',
          valid_until: '2026-11-20',
          lines: [line],
        },
        {
          id: 'current',
          supplier_id: 'b',
          supplier_name: 'B',
          title: 'Current',
          valid_until: '2026-11-21',
          lines: [line],
        },
        { id: 'missing', supplier_id: 'c', supplier_name: 'C', title: 'Missing', lines: [] },
      ],
    },
  };
  const html = renderToStaticMarkup(
    <RfqServiceComparison
      value={rfq}
      selection={{
        calculationDate: '2026-11-21',
        selected: new Set(['rfq:expired:line']),
        added: new Set(['rfq:current:line']),
        onToggle() {},
      }}
    />,
  );
  const checkboxes = html.match(/<input[^>]*>/g) ?? [];
  const expired = checkboxes.find((input) => input.includes('Old'))!;
  const imported = checkboxes.find((input) => input.includes('Current'))!;
  expect(expired).toContain('disabled');
  expect(expired).not.toContain('checked');
  expect(imported).toContain('disabled');
  expect(imported).toContain('checked');
  expect(html).toContain('Нет в котировке');
  expect(html).toContain('Уже в предложении');
  expect(html).toContain('200,00');
});
