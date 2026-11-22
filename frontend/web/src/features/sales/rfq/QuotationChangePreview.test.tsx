import { renderToStaticMarkup } from 'react-dom/server';
import { expect, it } from 'vitest';
import { QuotationChangePreview } from './QuotationChangePreview';
import type { ProposalLine } from '../proposals';
const before: ProposalLine = {
  id: 'line',
  label_id: 'label',
  unit: 'container',
  quantity_milli: 1000,
  route_segment_id: 'sea',
  sale: { currency: 'RUB', price_minor: 1500000, price_mode: 'net', vat_mode: 'exempt' },
  purchase: { currency: 'RUB', price_minor: 1000000, price_mode: 'net', vat_mode: 'exempt' },
  quote_source: { rfq_id: 'rfq', revision: 1, quote_id: 'quote', line_id: 'line' },
};
const incoming = {
  ...before,
  purchase: { ...before.purchase!, price_minor: 1200000 },
  quote_source: { ...before.quote_source!, revision: 2 },
};
it('previews the purchase, profit and margin while retaining the client price', () => {
  const html = renderToStaticMarkup(<QuotationChangePreview before={before} incoming={incoming} />);
  expect(html).toContain('12 000,00 RUB');
  expect(html).toContain('3 000,00 RUB');
  expect(html).toContain('33,33%');
  expect(html).toContain('20,00%');
  expect(html).toContain('сохранятся');
  expect(before.purchase?.price_minor).toBe(1000000);
});
it('warns when a volume change resets the client price', () => {
  const html = renderToStaticMarkup(
    <QuotationChangePreview before={before} incoming={{ ...incoming, quantity_milli: 2000 }} />,
  );
  expect(html).toContain('цена клиенту будет сброшена');
});
